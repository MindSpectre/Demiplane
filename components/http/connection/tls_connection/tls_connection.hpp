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
#include <boost/beast/core/error.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <http_enums.hpp>
#include <request_arena.hpp>

namespace demiplane::http {

    /**
     * @brief TLS connection: ssl::stream<beast::tcp_stream> + per-connection
     *        arena + cancel signal + the ALPN-negotiated protocol (spec §6.1).
     *
     * Satisfies StreamConnection — Http11Driver::serve drives it unchanged. The
     * TlsListener (PR 4 Task 10) constructs it on a per-connection strand, calls
     * handshake() (which records the negotiated protocol from ALPN), then
     * dispatches to the driver whose id() matches negotiated_protocol().
     *
     * Non-movable (composes the immovable RequestArena + cancellation_signal).
     */
    class TlsConnection : gears::Immutable {
    public:
        using stream_type = boost::asio::ssl::stream<boost::beast::tcp_stream>;

        TlsConnection(boost::asio::ip::tcp::socket socket,
                      boost::asio::ssl::context& ctx,
                      const std::size_t arena_size = 8192)
            : stream_{std::move(socket), ctx},
              arena_{arena_size} {
        }

        /// TLS handshake (server role). Records the ALPN-negotiated protocol.
        /// Returns the handshake error_code (empty on success). NOT in the
        /// Connection concept — the listener calls it before serve().
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

        void expires_after(const std::chrono::milliseconds ms) {
            boost::beast::get_lowest_layer(stream_).expires_after(ms);
        }

        boost::asio::awaitable<void> async_close();

        [[nodiscard]] boost::asio::cancellation_slot cancel_slot() noexcept {
            return signal_.slot();
        }

        void cancel() noexcept {
            signal_.emit(boost::asio::cancellation_type::terminal);
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
    };

}  // namespace demiplane::http
