#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast/core/error.hpp>
#include <connection_tracker.hpp>
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
     * LIFETIME CONTRACT: the caller MUST co_await drain_until(...) before
     * destroying the listener — spawned serve coroutines hold a tracker Handle
     * that references this listener's tracker, and call driver.serve() through
     * `this`.
     * // TODO(PR5): drain_until dispatches force-cancels but does not await their unwind; "safe to destroy after drain"
     * holds only on a single-threaded executor in v1. See ConnectionTracker::drain_until.
     */
    template <IsHttpDriver Driver>
    class TcpListener final : public ListenerBase {
    public:
        TcpListener(boost::asio::any_io_executor exec,
                    std::string host,
                    std::uint16_t port,
                    Driver driver,
                    std::size_t arena_size = 8192)
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
            for (;;) {
                auto strand = asio::make_strand(exec_);
                boost::beast::error_code ec;
                asio::ip::tcp::socket sock =
                    co_await acceptor_.async_accept(strand, asio::redirect_error(asio::use_awaitable, ec));
                if (ec == asio::error::operation_aborted) {
                    break;  // stop(): the run-coroutine's cancellation slot was emitted
                }
                if (ec) {
                    continue;  // transient accept error — keep accepting
                }
                auto conn   = std::make_shared<TcpConnection>(std::move(sock), arena_size_);
                auto handle = tracker_.register_connection(conn, strand);
                asio::co_spawn(
                    strand,
                    [this, &router, conn, h = std::move(handle)]() -> asio::awaitable<void> {
                        co_await driver_.serve(*conn, router);
                    },
                    asio::detached);
            }
            // The loop only exits on shutdown (operation_aborted). Close the
            // acceptor so new SYNs are REFUSED (ECONNREFUSED), not silently
            // backlogged + left unserved while we drain (spec §14.2; spike S4).
            boost::beast::error_code ignore;
            acceptor_.close(ignore);
            co_return;
        }

        boost::asio::awaitable<void> drain_until(const std::chrono::steady_clock::time_point deadline) override {
            co_await tracker_.drain_until(exec_, deadline);
        }

        [[nodiscard]] std::string bind_address() const override {
            return host_;
        }
        [[nodiscard]] std::uint16_t bound_port() const override {
            return acceptor_.local_endpoint().port();
        }

    private:
        boost::asio::any_io_executor exec_;
        std::string host_;
        std::uint16_t port_;
        Driver driver_;
        std::size_t arena_size_;
        boost::asio::ip::tcp::acceptor acceptor_;
        ConnectionTracker tracker_;
    };

}  // namespace demiplane::http
