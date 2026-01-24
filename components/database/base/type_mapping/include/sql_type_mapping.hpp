#pragma once

#include <concepts>
#include <string_view>
#include <type_traits>

#include <providers.hpp>

#include "gears_utils.hpp"

namespace demiplane::db {

    // Forward declaration - no hard dependency on SqlDialect
    class SqlDialect;


    // PRIMARY TYPE TRAIT: Maps C++ types to SQL type strings
    // Unspecialized = compile error (no silent fallbacks)


    template <typename T, Providers Provider>
    struct SqlTypeMapping;  // Primary template - intentionally undefined


    // CONCEPT: Check if type mapping exists for T and Provider


    template <typename T, Providers P>
    concept HasSqlTypeMapping = requires {
        { SqlTypeMapping<std::remove_cvref_t<T>, P>::sql_type } -> std::convertible_to<std::string_view>;
    };


    // COMPILE-TIME API: sql_type_for<T, Provider>()


    template <typename T, Providers P>
    constexpr std::string_view sql_type_for() {
        static_assert(HasSqlTypeMapping<T, P>,
                      "No SQL type mapping exists for this type and provider. "
                      "Add a specialization of SqlTypeMapping<T, Provider>.");
        return SqlTypeMapping<std::remove_cvref_t<T>, P>::sql_type;
    }


    // RUNTIME API: sql_type<T>(provider_enum)
    // Compile-time dispatch via constexpr switch


    template <typename T>
    constexpr std::string_view sql_type(const Providers provider) {
        switch (provider) {
            case Providers::None:
                assert(false && "Cannot infer SQL type: Table has no provider set.");
            case Providers::PostgreSQL:
                return sql_type_for<T, Providers::PostgreSQL>();
        }
        // Providers::None is invalid - no mappings exist
        std::unreachable();
    }


    // RUNTIME API: sql_type<T>(dialect) - dispatches via dialect.type()


    template <typename T>
    std::string_view sql_type(const SqlDialect& dialect);

    template <typename T>
    std::string_view sql_type(const SqlDialect* dialect);

}  // namespace demiplane::db

#include "../source/sql_type_mapping.inl"
