#pragma once

#include <cstdint>

namespace demiplane::db::postgres {

    struct OidTypeRegistry {
        // Numeric types
        constexpr static std::uint32_t oid_int2    = 21;
        constexpr static std::uint32_t oid_int4    = 23;
        constexpr static std::uint32_t oid_int8    = 20;
        constexpr static std::uint32_t oid_float4  = 700;
        constexpr static std::uint32_t oid_float8  = 701;
        constexpr static std::uint32_t oid_numeric = 1700;
        constexpr static std::uint32_t oid_money   = 790;


        // Character types
        constexpr static std::uint32_t oid_char    = 18;    // "char" (internal 1-byte type)
        constexpr static std::uint32_t oid_bpchar  = 1042;  // CHAR(n) / CHARACTER(n)
        constexpr static std::uint32_t oid_varchar = 1043;  // VARCHAR(n)
        constexpr static std::uint32_t oid_text    = 25;


        // Binary types
        constexpr static std::uint32_t oid_bytea = 17;


        // Boolean type
        constexpr static std::uint32_t oid_bool = 16;


        // Date/Time types
        constexpr static std::uint32_t oid_date        = 1082;
        constexpr static std::uint32_t oid_time        = 1083;
        constexpr static std::uint32_t oid_timetz      = 1266;
        constexpr static std::uint32_t oid_timestamp   = 1114;
        constexpr static std::uint32_t oid_timestamptz = 1184;
        constexpr static std::uint32_t oid_interval    = 1186;


        // UUID type
        constexpr static std::uint32_t oid_uuid = 2950;


        // JSON types
        constexpr static std::uint32_t oid_json  = 114;
        constexpr static std::uint32_t oid_jsonb = 3802;


        // Network types
        constexpr static std::uint32_t oid_inet    = 869;
        constexpr static std::uint32_t oid_cidr    = 650;
        constexpr static std::uint32_t oid_macaddr = 829;


        // Bit string types
        constexpr static std::uint32_t oid_bit    = 1560;
        constexpr static std::uint32_t oid_varbit = 1562;


        // Other types
        constexpr static std::uint32_t oid_xml = 142;
        constexpr static std::uint32_t oid_oid = 26;
    };

}  // namespace demiplane::db::postgres
