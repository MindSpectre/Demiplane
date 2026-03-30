#pragma once

#include <concepts>
#include <string_view>
#include <type_traits>

#include <providers.hpp>

namespace demiplane::db {


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
    [[nodiscard]] constexpr std::string_view sql_type_for() noexcept {
        static_assert(HasSqlTypeMapping<T, P>,
                      "No SQL type mapping exists for this type and provider. "
                      "Add a specialization of SqlTypeMapping<T, Provider>.");
        return SqlTypeMapping<std::remove_cvref_t<T>, P>::sql_type;
    }


    // RUNTIME API: sql_type<T>(provider_enum)
    // Compile-time dispatch via constexpr switch


    template <typename T>
    [[nodiscard]] constexpr std::string_view sql_type(const Providers provider) noexcept {
        switch (provider) {
            case Providers::None:
                std::unreachable();
            case Providers::PostgreSQL:
                return sql_type_for<T, Providers::PostgreSQL>();
        }
        // Providers::None is invalid - no mappings exist
        std::unreachable();
    }

}  // namespace demiplane::db

#include "detail/db_table_mapping.inl"
