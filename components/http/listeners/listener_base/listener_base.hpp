#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#include <boost/asio/awaitable.hpp>
#include <executor.hpp>
#include <router.hpp>

namespace demiplane::http {

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
