#pragma once

#include <chrono>
#include <cstddef>
#include <utility>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/beast/core/error.hpp>
#include <executor.hpp>
#include <http_enums.hpp>
#include <request_arena.hpp>

namespace demiplane::http {

    /**
     * @brief TLS connection: ssl::stream<Stream> + per-connection
     *        arena + cancel signal + the ALPN-negotiated protocol (spec §6.1).
     *
     * Satisfies IsStreamConnection — Http11Driver::serve drives it unchanged. The
     * TlsListener (PR 4 Task 10) constructs it on a per-connection strand, calls
     * handshake() (which records the negotiated protocol from ALPN), then
     * dispatches to the driver whose id() matches negotiated_protocol().
     *
     * Non-movable (composes the immovable RequestArena + cancellation_signal).
     */
    class TlsConnection : gears::Immutable {
    public:
        using stream_type = boost::asio::ssl::stream<Stream>;

        TlsConnection(Socket socket, boost::asio::ssl::context& ctx, const std::size_t arena_size = 8192)
            : stream_{std::move(socket), ctx},
              arena_{arena_size},
              watchdog_timer_{stream_.get_executor()} {
        }

        /// TLS handshake (server role). Records the ALPN-negotiated protocol.
        /// Returns the handshake error_code (empty on success). NOT in the
        /// IsConnection concept — the listener calls it before serve().
        boost::asio::awaitable<boost::beast::error_code>
        handshake(std::chrono::milliseconds timeout = std::chrono::seconds{10});

        [[nodiscard]] stream_type& stream() noexcept {
            return stream_;
        }

        [[nodiscard]] std::pmr::polymorphic_allocator<> arena_alloc() noexcept {
            return arena_.allocator();
        }

        void reset_request_arena() {
            arena_.reset();
        }

        /// Per-phase I/O deadline (IsConnection) — see TcpConnection: a plain
        /// strand-confined store enforced by the listener's deadline_watchdog.
        /// The TLS HANDSHAKE keeps its own beast per-op timeout internally
        /// (once per connection, off the request hot path).
        void set_deadline_after(const std::chrono::milliseconds ms) noexcept {
            deadline_ = std::chrono::steady_clock::now() + ms;
        }

        [[nodiscard]] std::chrono::steady_clock::time_point deadline() const noexcept {
            return deadline_;
        }

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

        /// Force-cancel this connection (graceful-shutdown deadline); dispatched
        /// onto the connection's strand by the ConnectionTracker (D2).
        /// LEVEL-TRIGGERED like TcpConnection::cancel(): emit() covers a parked
        /// slot-bound op; closing the lowest layer covers the unbound windows —
        /// including the ENTIRE TLS handshake, which never binds the conn slot
        /// (the SSL state machine surfaces the transport close as a clean
        /// handshake error, the same path beast's handshake timeout uses).
        void cancel() noexcept {
            signal_.emit(boost::asio::cancellation_type::terminal);
            boost::beast::get_lowest_layer(stream_).close();
        }

        [[nodiscard]] boost::asio::ip::address remote_address() const {
            return boost::beast::get_lowest_layer(stream_).socket().remote_endpoint().address();
        }

        [[nodiscard]] Protocol negotiated_protocol() const noexcept {
            return negotiated_protocol_;
        }

        [[nodiscard]] static bool is_secure() noexcept {
            return true;
        }

    private:
        stream_type stream_;
        RequestArena arena_;
        boost::asio::cancellation_signal signal_;
        Protocol negotiated_protocol_ = Protocol::http1;
        // Strand-confined deadline state (driver writes, watchdog reads).
        std::chrono::steady_clock::time_point deadline_{std::chrono::steady_clock::time_point::max()};
        boost::asio::steady_timer watchdog_timer_;
        bool serve_finished_ = false;
    };

}  // namespace demiplane::http
