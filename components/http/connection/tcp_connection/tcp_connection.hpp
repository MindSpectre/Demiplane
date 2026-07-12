#pragma once

#include <atomic>
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
        // Raw Socket, NOT beast::basic_stream (README Finding 15): with the
        // per-op timeouts long gone (deadline sweep) and the write path flat,
        // the beast wrapper was pure forwarding shell — ~1.3% of all cycles
        // of read_some/write_some/rate-policy plumbing per op. The driver
        // only needs AsyncReadStream/AsyncWriteStream, which the socket is.
        // (TlsConnection keeps beast's stream underneath ssl::stream — its
        // handshake timeout machinery is load-bearing there, once per conn.)
        using stream_type = Socket;

        explicit TcpConnection(Socket socket, const std::size_t arena_size = 8192)
            : stream_{std::move(socket)},
              arena_{arena_size} {
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

        /// Per-phase I/O deadline (IsConnection). Written by the driver on the
        /// connection's own context; READ by the ConnectionTracker's sweep
        /// from the listener's home context — hence the relaxed atomic (a bare
        /// store/load on x86, still effectively free on the hot path).
        /// Replaces beast's expires_after — that armed timer.async_wait +
        /// cancel + an aborted-handler dispatch around EVERY I/O op (~18% of
        /// hot-path throughput measured). Enforcement is the tracker sweep:
        /// ONE timer per listener, not per connection — per-connection timers
        /// measurably stall single-runner workers (README Finding 13).
        void set_deadline_after(const std::chrono::milliseconds ms) noexcept {
            deadline_.store((std::chrono::steady_clock::now() + ms).time_since_epoch().count(),
                            std::memory_order_relaxed);
        }

        [[nodiscard]] std::chrono::steady_clock::time_point deadline() const noexcept {
            return std::chrono::steady_clock::time_point{
                std::chrono::steady_clock::duration{deadline_.load(std::memory_order_relaxed)}};
        }

        boost::asio::awaitable<void, Strand> async_close();

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
        /// the next I/O boundary. Both calls are no-throw (error_code form).
        void cancel() noexcept {
            signal_.emit(boost::asio::cancellation_type::terminal);  // aborts a slot-bound parked op now
            boost::system::error_code ignore;
            stream_.close(ignore);  // level-trigger for the unbound windows
        }

        [[nodiscard]] boost::asio::ip::address remote_address() const {
            return stream_.remote_endpoint().address();
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
        // Driver writes (own context), tracker sweep reads (home context).
        std::atomic<std::chrono::steady_clock::duration::rep> deadline_{
            std::chrono::steady_clock::time_point::max().time_since_epoch().count()};
    };

}  // namespace demiplane::http
