#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <router.hpp>

namespace demiplane::http {

    /**
     * @brief Deadline supervisor for one connection; runs on its strand.
     *
     * Replaces beast's per-op stream timeouts on the request hot path. Those
     * arm timer.async_wait + cancel + an aborted-handler dispatch around EVERY
     * I/O op — removing them measured +18% throughput at pipeline depth 1.
     * This watchdog costs one timer op per `tick` per CONNECTION instead: the
     * driver stamps conn.set_deadline_after(phase_timeout) (a plain store) and
     * the watchdog force-cancels once the deadline passes — the same
     * strand-serialized conn.cancel() kill path the tracker's drain uses.
     * Enforcement granularity is `tick`, not per-op-exact; config timeouts are
     * seconds, so half-second slack is inside their tolerance.
     *
     * Lifecycle: the serve wrapper calls conn->end_watchdog() when the session
     * coroutine finishes; that cancels the pending wait and the loop exits on
     * the flag. All state is strand-confined — no atomics. After a deadline
     * kill the loop keeps ticking (cancel is idempotent) until serve() unwinds
     * and sets the flag; it never outlives `conn` (it owns a shared_ptr).
     */
    template <typename Conn>
    boost::asio::awaitable<void>
    deadline_watchdog(std::shared_ptr<Conn> conn,
                      const std::chrono::milliseconds tick = std::chrono::milliseconds{500}) {
        namespace asio = boost::asio;
        // Cancellation as state: an executor-level cancel must not throw out of
        // a detached coroutine mid-teardown.
        co_await asio::this_coro::throw_if_cancelled(false);
        const auto cancel_state = co_await asio::this_coro::cancellation_state;
        auto& timer             = conn->watchdog_timer();
        boost::system::error_code ec;  // aborted wakes are a signal here, not an error
        while (!conn->serve_finished() && cancel_state.cancelled() == asio::cancellation_type::none) {
            timer.expires_after(tick);
            co_await timer.async_wait(asio::redirect_error(asio::use_awaitable, ec));
            if (conn->serve_finished())
                break;
            if (std::chrono::steady_clock::now() >= conn->deadline())
                conn->cancel();  // level-triggered kill; serve() unwinds and sets the flag
        }
    }

    /**
     * @brief Type-erased listener interface (spec §7.1).
     *
     * The ONLY virtual seam in the runtime path: the Server (PR 5) owns
     * std::vector<std::unique_ptr<ListenerBase>>. Concrete listeners
     * (TcpListener<Driver>, TlsListener<Drivers...>, QuicListener<Http3Driver>)
     * are templated on their driver(s); ListenerBase erases that so the Server
     * does not template on the protocol set.
     *
     * Lifecycle: bind() synchronously (throws on failure) → the caller
     * co_spawns run(router) bound to a cancellation slot → on shutdown the
     * caller emits terminal on that slot (stops accepting) and awaits
     * drain_until(deadline) (in-flight requests finish or are force-cancelled).
     */
    class ListenerBase : gears::Immutable {
    public:
        ListenerBase()          = default;
        virtual ~ListenerBase() = default;

        /// Open + bind + listen. Synchronous; throws boost::system::system_error
        /// (or std::system_error for cert load) on failure — surfaced immediately.
        virtual void bind() = 0;

        /// Accept loop until the associated cancellation slot is emitted.
        virtual boost::asio::awaitable<void> run(Router& router) = 0;

        /// Wait for in-flight connections to finish, force-cancelling whatever
        /// remains at `deadline`. Delegates to the listener's ConnectionTracker.
        virtual boost::asio::awaitable<void> drain_until(std::chrono::steady_clock::time_point deadline) = 0;

        /// Tracked in-flight connections (serve() coroutines whose tracker Handle
        /// is still alive). Teardown barrier: after drain_until, wait for this to
        /// reach 0 before stopping the executor or destroying the listener —
        /// drain only dispatches force-cancels; the unwinds land in later turns.
        [[nodiscard]] virtual std::size_t in_flight() const noexcept = 0;

        [[nodiscard]] virtual std::string bind_address() const = 0;
        [[nodiscard]] virtual std::uint16_t bound_port() const = 0;  // for tests on :0
    };

}  // namespace demiplane::http
