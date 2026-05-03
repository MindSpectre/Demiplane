#pragma once
#include <bit>
#include <concepts>

namespace demiplane::gears {
    // Network byte order is big-endian
    // These functions convert between host and network byte order

    /// Network-to-host conversion for unsigned integers; no-op on big-endian
    /// platforms, byteswap on little-endian.
    template <std::unsigned_integral T>
    constexpr T ntoh(const T net) noexcept {
        if constexpr (std::endian::native == std::endian::big) {
            return net;
        } else {
            return std::byteswap(net);
        }
    }

    /// Host-to-network conversion — symmetric to `ntoh`.
    template <std::unsigned_integral T>
    constexpr T hton(const T host) noexcept {
        return ntoh(host);
    }
}