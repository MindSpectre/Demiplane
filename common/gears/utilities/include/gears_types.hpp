#pragma once

#include <cstdint>

namespace demiplane::gears::literals {
    // Byte
    constexpr unsigned long long operator""_b(const unsigned long long value) {
        return value;
    }

    // Kilobyte
    constexpr unsigned long long operator""_kb(const unsigned long long value) {
        return value * 1024ULL;
    }

    // Megabyte
    constexpr unsigned long long operator""_mb(const unsigned long long value) {
        return value * 1024ULL * 1024ULL;
    }

    // Gigabyte
    constexpr unsigned long long operator""_gb(const unsigned long long value) {
        return value * 1024ULL * 1024ULL * 1024ULL;
    }

    // Terabyte
    constexpr unsigned long long operator""_tb(const unsigned long long value) {
        return value * 1024ULL * 1024ULL * 1024ULL * 1024ULL;
    }
} // namespace demiplane::gears::literals
