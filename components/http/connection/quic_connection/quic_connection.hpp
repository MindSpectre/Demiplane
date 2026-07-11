#pragma once

#include <chrono>
#include <cstddef>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/ip/address.hpp>
#include <http_enums.hpp>
#include <request_arena.hpp>

namespace demiplane::http {

    /// QUIC connection — SCAFFOLD (spec §6.1). ngtcp2 state lands when h3 is
    /// implemented. Satisfies IsConnection (arena + lifecycle) but NOT
    /// IsStreamConnection (QUIC is not a single byte stream).
    class QuicConnection : gears::Immutable {
    public:
        explicit QuicConnection(const std::size_t arena_size = 8192)
            : arena_{arena_size} {
        }

        [[nodiscard]] std::pmr::polymorphic_allocator<> arena_alloc() noexcept {
            return arena_.allocator();
        }
        void reset_request_arena() {
            arena_.reset();
        }

        /// Scaffold no-op (IsConnection); QUIC timeout enforcement arrives
        /// with the real h3 transport. See TcpConnection::set_deadline_after.
        void set_deadline_after(std::chrono::milliseconds) noexcept {
            gears::force_non_const(this);
        }

        boost::asio::awaitable<void> async_close() {
            gears::force_non_const(this);
            co_return;
        }

        [[nodiscard]] boost::asio::cancellation_slot cancel_slot() noexcept {
            return signal_.slot();
        }

        [[nodiscard]] boost::asio::ip::address remote_address() const {
            gears::force_non_static(this);
            return {};
        }

        [[nodiscard]] static Protocol negotiated_protocol() noexcept {
            return Protocol::http3;
        }

        [[nodiscard]] static bool is_secure() noexcept {
            return true;
        }

    private:
        RequestArena arena_;
        boost::asio::cancellation_signal signal_;
    };

}  // namespace demiplane::http
