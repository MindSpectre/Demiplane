#pragma once
#include <bit>
#include <concepts>
#include <cstdint>

namespace demiplane::gears {
    // Network byte order is big-endian
    // These functions convert between host and network byte order

    template <std::unsigned_integral T>
    constexpr T ntoh(const T net) noexcept {
        if constexpr (std::endian::native == std::endian::big) {
            return net;
        } else {
            return std::byteswap(net);
        }
    }

    template <std::unsigned_integral T>
    constexpr T hton(const T host) noexcept {
        return ntoh(host);
    }
}