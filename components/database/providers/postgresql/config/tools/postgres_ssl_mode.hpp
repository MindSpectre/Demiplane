#pragma once
#include <cstdint>
#include <string_view>
#include <utility>

namespace demiplane::db::postgres {

    enum class SslMode : std::uint8_t {
        DISABLE     = 0,
        ALLOW       = 1,
        PREFER      = 2,
        REQUIRE     = 3,
        VERIFY_CA   = 4,
        VERIFY_FULL = 5
    };

    template <typename StringT = std::string_view>
    [[nodiscard]] constexpr StringT ssl_mode_to_string_t(const SslMode mode) noexcept {
        switch (mode) {
            case SslMode::DISABLE:
                return "disable";
            case SslMode::ALLOW:
                return "allow";
            case SslMode::PREFER:
                return "prefer";
            case SslMode::REQUIRE:
                return "require";
            case SslMode::VERIFY_CA:
                return "verify-ca";
            case SslMode::VERIFY_FULL:
                return "verify-full";
        }
        std::unreachable();
    }

    template <typename T>
        requires std::is_integral_v<T>
    [[nodiscard]] constexpr SslMode ssl_mode_from_int(const T value) noexcept {
        switch (value) {
            case 0:
                return SslMode::DISABLE;
            case 1:
                return SslMode::ALLOW;
            case 2:
                return SslMode::PREFER;
            case 3:
                return SslMode::REQUIRE;
            case 4:
                return SslMode::VERIFY_CA;
            case 5:
                return SslMode::VERIFY_FULL;
            default:
                return SslMode::PREFER;
        }
        std::unreachable();
    }
}  // namespace demiplane::db::postgres
