#pragma once
#include <cstdint>
#include <utility>

namespace demiplane::db {
    enum class SupportedProviders : std::uint8_t {
        None = 0,
        PostgreSQL,
    };

    constexpr const char* to_string(SupportedProviders provider) noexcept {
        switch (provider) {
            case SupportedProviders::None:
                return "None";
            case SupportedProviders::PostgreSQL:
                return "PostgreSQL";
        }
        std::unreachable();
    }
}  // namespace demiplane::db
