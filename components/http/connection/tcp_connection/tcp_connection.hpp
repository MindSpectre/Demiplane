#pragma once

#include <chrono>
#include <cstddef>
#include <utility>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <http_enums.hpp>
#include <request_arena.hpp>

namespace demiplane::http {

    /**
     * @brief Plain-TCP connection: beast::tcp_stream + per-connection arena +
     *        cancel signal (spec §6.1).
     *
     * Non-movable (composes the immovable RequestArena + cancellation_signal);
     * the TcpListener (later PR) constructs it in place / on the heap per accept.
     */
    class TcpConnection : gears::Immutable {
    public:
        using stream_type = boost::beast::tcp_stream;

        explicit TcpConnection(boost::asio::ip::tcp::socket socket, const std::size_t arena_size = 8192)
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

        void expires_after(const std::chrono::milliseconds ms) {
            stream_.expires_after(ms);
        }

        boost::asio::awaitable<void> async_close();

        [[nodiscard]] boost::asio::cancellation_slot cancel_slot() noexcept {
            return signal_.slot();
        }

        /// Force-cancel this connection's in-flight I/O (graceful shutdown). The
        /// ConnectionTracker dispatches this onto the connection's strand (D2),
        /// so emit() is serialized with the serve coroutine's I/O on that strand.
        void cancel() noexcept {
            signal_.emit(boost::asio::cancellation_type::terminal);
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
    };

}  // namespace demiplane::http
