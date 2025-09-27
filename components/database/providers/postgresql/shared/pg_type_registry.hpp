#pragma once

#include <cstdint>

namespace demiplane::db {

    struct PgTypeRegistry {
        std::uint32_t oid_bool        = 16;
        std::uint32_t oid_int2        = 21;
        std::uint32_t oid_int4        = 23;
        std::uint32_t oid_int8        = 20;
        std::uint32_t oid_float8      = 701;
        std::uint32_t oid_text        = 25;
        std::uint32_t oid_bytea       = 17;
        std::uint32_t oid_timestamptz = 1184;
    };
}  // namespace demiplane::db
