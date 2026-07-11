#pragma once

#include <chrono>
#include <concepts>
#include <memory_resource>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/ip/address.hpp>
#include <http_enums.hpp>

namespace demiplane::http {

    /**
     * @brief One peer's byte stream + lifecycle, duck-typed (spec §6.1).
     *
     * Concrete connections (TcpConnection, the test fake, QuicConnection later)
     * are value types composing a RequestArena + per-transport state. No virtual
     * base: drivers template their serve() on the connection type, so the stream
     * type is chosen at compile time — no dynamic_cast, no per-byte vcall.
     */
    template <typename T>
    concept IsConnection = requires(T& t, std::chrono::milliseconds ms) {
        { t.arena_alloc() } -> std::same_as<std::pmr::polymorphic_allocator<>>;
        { t.reset_request_arena() } -> std::same_as<void>;
        // Deadline STORE, not a beast per-op timeout. beast's expires_after arms
        // timer.async_wait + cancel + an aborted-handler dispatch around EVERY
        // I/O op — measured at ~18% of throughput on the h1 hot path. Drivers
        // stamp a per-phase deadline (plain store, strand-serialized) and the
        // listener's deadline_watchdog enforces it at ~tick granularity.
        { t.set_deadline_after(ms) } -> std::same_as<void>;
        { t.async_close() } -> std::same_as<boost::asio::awaitable<void>>;
        { t.cancel_slot() } -> std::same_as<boost::asio::cancellation_slot>;
        { t.remote_address() } -> std::same_as<boost::asio::ip::address>;
        { t.negotiated_protocol() } -> std::same_as<Protocol>;
        { t.is_secure() } -> std::same_as<bool>;
    };

    /// An IsConnection that also exposes a Beast-compatible byte stream. Http11Driver
    /// requires this (it drives async_read/async_write on stream()).
    template <typename T>
    concept IsStreamConnection = IsConnection<T> && requires(T& t) {
        typename T::stream_type;
        { t.stream() } -> std::same_as<typename T::stream_type&>;
    };

}  // namespace demiplane::http
