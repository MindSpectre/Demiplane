#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <demiplane/chrono>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast/core/error.hpp>
#include <connection_tracker.hpp>
#include <executor.hpp>
#include <http_driver_concept.hpp>
#include <listener_base.hpp>
#include <router.hpp>
#include <tcp_connection.hpp>

namespace demiplane::http {

    /**
     * @brief Plain-TCP listener for one driver (spec §7.3).
     *
     * Accepts each connection ONTO A FRESH STRAND (D7 — beast::tcp_stream caches
     * its executor at construction, so the socket must already be strand-bound),
     * heap-allocates the (non-movable) TcpConnection as a shared_ptr, registers
     * it with the tracker, and co_spawns driver.serve() on that strand.
     *
     * LIFETIME CONTRACT: the caller MUST co_await drain_until(...) AND then wait
     * for in_flight() == 0 before destroying the listener — spawned serve
     * coroutines hold a tracker Handle that references this listener's tracker,
     * and call driver.serve() through `this`.
     * WARN: drain_until only DISPATCHES force-cancels; it does not await the cancelled serve()
     * coroutines' unwind on ANY executor (cancels + unwinds run in later executor turns). The Server
     * honors this in graceful_shutdown() phase 2.5 by polling in_flight() == 0 after the drain; direct
     * users (the integration fixture) must do the same. See ConnectionTracker::drain_until.
     */
    template <IsHttpDriver Driver>
    class TcpListener final : public ListenerBase {
    public:
        TcpListener(Executor exec,
                    std::string host,
                    const std::uint16_t port,
                    Driver driver,
                    const std::size_t arena_size = 8192)
            : exec_{std::move(exec)},
              host_{std::move(host)},
              port_{port},
              driver_{std::move(driver)},
              arena_size_{arena_size},
              acceptor_{exec_} {
        }

        void bind() override {
            const boost::asio::ip::tcp::endpoint ep{boost::asio::ip::make_address(host_), port_};
            acceptor_.open(ep.protocol());
            acceptor_.set_option(boost::asio::socket_base::reuse_address(true));
            acceptor_.bind(ep);  // throws boost::system::system_error on failure
            acceptor_.listen(boost::asio::socket_base::max_listen_connections);
        }

        boost::asio::awaitable<void> run(Router& router) override {
            namespace asio = boost::asio;
            // Cancellation is handled as STATE, not exceptions: with the default
            // throw_if_cancelled, a stop emitted between suspensions would make
            // the next co_await throw and skip the acceptor close below (and,
            // where run()'s completion is awaited through a future, rethrow there).
            co_await asio::this_coro::throw_if_cancelled(false);
            const auto cancel_state = co_await asio::this_coro::cancellation_state;
            std::optional<asio::steady_timer> backoff;  // error path only — lazy
            unsigned consecutive_errors = 0;

            // WARN: multi-worker — an emit landing between the loop-top state
            // check and async_accept installing its cancel handler would be
            // edge-lost. The check→install section runs in ONE executor turn,
            // so the window is closed whenever the emit is serialized with
            // this coroutine's turns: the Server spawns run() on a dedicated
            // strand and DISPATCHES the stop emit onto it (PR 5, D5). Direct
            // users on a multi-threaded executor must do the same.
            while (cancel_state.cancelled() == asio::cancellation_type::none) {
                Strand strand = asio::make_strand(exec_);
                boost::beast::error_code ec;
                // Yields a Socket (strand-bound), not a tcp::socket — assigning to
                // tcp::socket would convert the executor back into any_io_executor
                // and re-erase the whole hot path.
                Socket sock = co_await acceptor_.async_accept(strand, asio::redirect_error(asio::use_awaitable, ec));
                if (ec == asio::error::operation_aborted) {
                    break;  // stop(): the run-coroutine's cancellation slot was emitted
                }
                if (ec == asio::error::bad_descriptor || ec == asio::error::operation_not_supported ||
                    ec == asio::error::invalid_argument) {
                    break;  // acceptor unusable — retrying can only repeat the error
                }
                if (ec) {  // transient: ECONNABORTED / EMFILE / ENOBUFS / ...
                    // A couple of retries are free (accept-storm hiccups resolve
                    // instantly); persistent failure backs off exponentially so a
                    // dead-resource state (fd exhaustion) doesn't hot-spin the
                    // worker. Timer syscalls only on this already-failing path.
                    if (++consecutive_errors > 2) {
                        if (!backoff) {
                            backoff.emplace(exec_);
                        }
                        // 1ms, 2ms, 4ms, ... capped at 1024ms: resource-exhaustion
                        // errors (EMFILE) resolve on operator timescales, not
                        // microseconds. attempt is 0-based; consecutive_errors is >2
                        // here, so consecutive_errors - 3 never underflows.
                        backoff->expires_after(demiplane::chrono::exponential_backoff(
                            consecutive_errors - 3, std::chrono::milliseconds{1}, std::chrono::milliseconds{1024}));
                        boost::beast::error_code tec;
                        co_await backoff->async_wait(asio::redirect_error(asio::use_awaitable, tec));
                        // tec == operation_aborted ⇒ cancelled mid-sleep ⇒ the
                        // latched state exits the loop above.
                    }
                    continue;
                }
                consecutive_errors = 0;
                // Nagle holds every small segment after the first unacked one.
                // The driver batches pipelined responses into one write, but a
                // batch boundary (and every non-pipelined exchange) is still a
                // small segment that would wait ~40ms on the peer's delayed ACK.
                // Measured before batching: 30k → 343k req/s at depth 16.
                // Failure to set it is ignored — a socket that rejects the
                // option still serves correctly, just slower.
                {
                    boost::beast::error_code nd_ec;
                    sock.set_option(asio::ip::tcp::no_delay(true), nd_ec);
                }
                auto conn   = std::make_shared<TcpConnection>(std::move(sock), arena_size_);
                auto handle = tracker_.register_connection(conn, strand);
                asio::co_spawn(
                    strand,
                    [this, &router, conn, h = std::move(handle)]() -> asio::awaitable<void> {
                        try {
                            co_await driver_.serve(*conn, router);
                        } catch (...) {  // serve() is noexcept in practice; the watchdog
                        }  // must still be released if that ever changes
                        conn->end_watchdog();  // strand-serialized with the watchdog's turns
                    },
                    asio::detached);
                // Deadline supervisor for this connection (see listener_base.hpp).
                // Holds its own shared_ptr — safe past the tracker Handle's release.
                asio::co_spawn(strand, deadline_watchdog(conn), asio::detached);
            }
            // Reached on EVERY exit path (stop, fatal accept error). Close the
            // acceptor so new SYNs are REFUSED (ECONNREFUSED), not silently
            // backlogged + left unserved while we drain (spec §14.2; spike S4).
            boost::beast::error_code ignore;
            acceptor_.close(ignore);
            co_return;
        }

        boost::asio::awaitable<void> drain_until(const std::chrono::steady_clock::time_point deadline) override {
            co_await tracker_.drain_until(exec_, deadline);
        }

        [[nodiscard]] std::size_t in_flight() const noexcept override {
            return tracker_.in_flight();
        }

        [[nodiscard]] std::string bind_address() const override {
            return host_;
        }
        [[nodiscard]] std::uint16_t bound_port() const override {
            return acceptor_.local_endpoint().port();
        }

    private:
        Executor exec_;
        std::string host_;
        std::uint16_t port_;
        Driver driver_;
        std::size_t arena_size_;
        Acceptor acceptor_;
        ConnectionTracker tracker_;
    };

}  // namespace demiplane::http
