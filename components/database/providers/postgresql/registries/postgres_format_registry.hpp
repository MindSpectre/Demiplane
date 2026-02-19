#pragma once
#include <cstdint>

namespace demiplane::db::postgres {
    struct FormatRegistry {
        constexpr static std::uint32_t text   = 0;
        constexpr static std::uint32_t binary = 1;
    };
}  // namespace demiplane::db::postgres
