#pragma once

#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <gears_templates.hpp>
#include <postgres_sql_type_registry.hpp>
#include <sql_type_mapping.hpp>

namespace demiplane::db {

    template <>
    struct SqlTypeMapping<bool, SupportedProviders::PostgreSQL> {
        static constexpr std::string_view sql_type = postgres::SqlTypeRegistry::boolean;
    };

    template <>
    struct SqlTypeMapping<char, SupportedProviders::PostgreSQL> {
        static constexpr std::string_view sql_type = postgres::SqlTypeRegistry::char_type<1>();
    };

    template <>
    struct SqlTypeMapping<std::int16_t, SupportedProviders::PostgreSQL> {
        static constexpr std::string_view sql_type = postgres::SqlTypeRegistry::smallint;
    };

    template <>
    struct SqlTypeMapping<std::int32_t, SupportedProviders::PostgreSQL> {
        static constexpr std::string_view sql_type = postgres::SqlTypeRegistry::integer;
    };

    template <>
    struct SqlTypeMapping<std::int64_t, SupportedProviders::PostgreSQL> {
        static constexpr std::string_view sql_type = postgres::SqlTypeRegistry::bigint;
    };

    template <>
    struct SqlTypeMapping<std::uint16_t, SupportedProviders::PostgreSQL> {
        static constexpr std::string_view sql_type = postgres::SqlTypeRegistry::integer;
    };

    template <>
    struct SqlTypeMapping<std::uint32_t, SupportedProviders::PostgreSQL> {
        static constexpr std::string_view sql_type = postgres::SqlTypeRegistry::bigint;
    };

    template <>
    struct SqlTypeMapping<std::uint64_t, SupportedProviders::PostgreSQL> {
        static constexpr std::string_view sql_type = postgres::SqlTypeRegistry::numeric<20, 0>();
    };

    template <>
    struct SqlTypeMapping<float, SupportedProviders::PostgreSQL> {
        static constexpr std::string_view sql_type = postgres::SqlTypeRegistry::real;
    };

    template <>
    struct SqlTypeMapping<double, SupportedProviders::PostgreSQL> {
        static constexpr std::string_view sql_type = postgres::SqlTypeRegistry::double_precision;
    };

    template <>
    struct SqlTypeMapping<std::string, SupportedProviders::PostgreSQL> {
        static constexpr std::string_view sql_type = postgres::SqlTypeRegistry::text;
    };

    template <>
    struct SqlTypeMapping<std::string_view, SupportedProviders::PostgreSQL> {
        static constexpr std::string_view sql_type = postgres::SqlTypeRegistry::text;
    };

    template <>
    struct SqlTypeMapping<std::vector<std::uint8_t>, SupportedProviders::PostgreSQL> {
        static constexpr std::string_view sql_type = postgres::SqlTypeRegistry::bytea;
    };

    template <>
    struct SqlTypeMapping<std::span<const std::uint8_t>, SupportedProviders::PostgreSQL> {
        static constexpr std::string_view sql_type = postgres::SqlTypeRegistry::bytea;
    };

}  // namespace demiplane::db

namespace demiplane::db::postgres {

    template <typename T>
    constexpr std::string_view sql_type_for() {
        return db::sql_type_for<T, SupportedProviders::PostgreSQL>();
    }

    namespace detail {
        template <typename T>
        struct has_postgres_mapping {
            // std::monostate represents NULL and doesn't need a SQL type mapping
            static constexpr bool value =
                std::is_same_v<T, std::monostate> || db::HasSqlTypeMapping<T, SupportedProviders::PostgreSQL>;
        };
    }  // namespace detail

    // This will cause a compile error if any FieldValue type is missing a PostgreSQL mapping
    static_assert(gears::all_variant_types_satisfy_v<FieldValue, detail::has_postgres_mapping>,
                  "Missing PostgreSQL SQL type mapping for one or more FieldValue types. "
                  "Add SqlTypeMapping<T, SupportedProviders::PostgreSQL> specialization for the missing type(s).");

}  // namespace demiplane::db::postgres
