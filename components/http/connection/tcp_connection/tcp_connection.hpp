#pragma once

#include <chrono>
#include <cstddef>
#include <utility>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/steady_timer.hpp>
#include <executor.hpp>
#include <http_enums.hpp>
#include <request_arena.hpp>

namespace demiplane::http {

    /**
     * @brief Plain-TCP connection: strand-bound beast stream + per-connection
     *        arena + cancel signal (spec §6.1).
     *
     * The stream is `Stream`, not `beast::tcp_stream`. The latter hard-codes
     * `any_io_executor`, which put a type-erased dispatch on every I/O op of the
     * request hot path — 14.25% of cycles by `perf`. See <executor.hpp>.
     *
     * Non-movable (composes the immovable RequestArena + cancellation_signal);
     * the TcpListener (later PR) constructs it in place / on the heap per accept.
     */
    class TcpConnection : gears::Immutable {
    public:
        using stream_type = Stream;

        explicit TcpConnection(Socket socket, const std::size_t arena_size = 8192)
            : stream_{std::move(socket)},
              arena_{arena_size},
              watchdog_timer_{stream_.get_executor()} {
        }

        [[nodiscard]] stream_type& stream() noexcept {
            return stream_;
        }

        [[nodiscard]] std::pmr::polymorphic_allocator<> arena_alloc() noexcept {
            return arena_.allocator();
        }

        void reset_request_arena() {
            arena_.reset();
        }

        /// Per-phase I/O deadline (IsConnection). A plain store: the driver and
        /// the listener's deadline_watchdog both run on this connection's
        /// strand, so no atomics. Replaces beast's expires_after — that armed
        /// timer.async_wait + cancel + an aborted-handler dispatch around EVERY
        /// I/O op (~18% of hot-path throughput measured); the watchdog costs
        /// one timer op per tick per connection instead.
        void set_deadline_after(const std::chrono::milliseconds ms) noexcept {
            deadline_ = std::chrono::steady_clock::now() + ms;
        }

        [[nodiscard]] std::chrono::steady_clock::time_point deadline() const noexcept {
            return deadline_;
        }

        /// Watchdog plumbing (strand-confined, driven by listener_base's
        /// deadline_watchdog). serve-wrapper calls end_watchdog() when the
        /// session coroutine finishes; the watchdog wakes and exits.
        [[nodiscard]] boost::asio::steady_timer& watchdog_timer() noexcept {
            return watchdog_timer_;
        }
        [[nodiscard]] bool serve_finished() const noexcept {
            return serve_finished_;
        }
        void end_watchdog() noexcept {
            serve_finished_ = true;
            try {
                watchdog_timer_.cancel();
            } catch (...) {  // cancel() throws only on closed services during teardown
            }
        }

        boost::asio::awaitable<void> async_close();

        [[nodiscard]] boost::asio::cancellation_slot cancel_slot() noexcept {
            return signal_.slot();
        }

        /// Force-cancel this connection (graceful-shutdown deadline). The
        /// ConnectionTracker dispatches this onto the connection's strand (D2),
        /// so it is serialized with the serve coroutine's I/O on that strand.
        /// LEVEL-TRIGGERED: emit() alone is edge-triggered (fires only a
        /// currently-installed slot handler, no latching) and the driver binds
        /// the slot per-op — an emit landing mid router.dispatch / between ops
        /// would be silently lost and the connection would outlive the drain.
        /// Closing the stream fails the current AND every future I/O op
        /// (operation_aborted / bad_descriptor), so the kill always lands at
        /// the next I/O boundary. Both calls are no-throw (beast's close() is
        /// socket.close(ec) + a try/catch'd timer cancel internally).
        void cancel() noexcept {
            signal_.emit(boost::asio::cancellation_type::terminal);  // aborts a slot-bound parked op now
            stream_.close();                                         // level-trigger for the unbound windows
        }

        [[nodiscard]] boost::asio::ip::address remote_address() const {
            return stream_.socket().remote_endpoint().address();
        }

        [[nodiscard]] static Protocol negotiated_protocol() noexcept {
            return Protocol::http1;
        }

        [[nodiscard]] static bool is_secure() noexcept {
            return false;
        }

    private:
        stream_type stream_;
        RequestArena arena_;
        boost::asio::cancellation_signal signal_;
        // Strand-confined deadline state (driver writes, watchdog reads).
        std::chrono::steady_clock::time_point deadline_{std::chrono::steady_clock::time_point::max()};
        boost::asio::steady_timer watchdog_timer_;
        bool serve_finished_ = false;
    };

}  // namespace demiplane::http
