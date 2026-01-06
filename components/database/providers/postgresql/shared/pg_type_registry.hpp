#pragma once

#include <cstdint>

namespace demiplane::db::postgres {

    struct TypeRegistry {
        constexpr static std::uint32_t oid_bool        = 16;
        constexpr static std::uint32_t oid_int2        = 21;
        constexpr static std::uint32_t oid_int4        = 23;
        constexpr static std::uint32_t oid_int8        = 20;
        constexpr static std::uint32_t oid_float4      = 700;
        constexpr static std::uint32_t oid_float8      = 701;
        constexpr static std::uint32_t oid_text        = 25;
        constexpr static std::uint32_t oid_bytea       = 17;
        constexpr static std::uint32_t oid_timestamptz = 1184;
    };
}  // namespace demiplane::db::postgres
