#pragma once

#include <string_view>
#include <type_traits>

#include <gears_types.hpp>

#include "db_constraints.hpp"

namespace demiplane::db {

    template <typename CppType, gears::FixedString Name, typename... Constraints>
    class StaticFieldSchema {
    public:
        using value_type = CppType;

        static constexpr auto name_literal = Name;

        static constexpr std::string_view name() noexcept {
            return std::string_view{Name};
        }

        // Simple tag constraints
        static constexpr bool is_primary_key = (std::is_same_v<Constraints, constraints::PrimaryKey> || ...);
        static constexpr bool is_nullable    = !(std::is_same_v<Constraints, constraints::NotNull> || ...);
        static constexpr bool is_unique      = (std::is_same_v<Constraints, constraints::Unique> || ...);
        static constexpr bool is_indexed     = (std::is_same_v<Constraints, constraints::Indexed> || ...);

        // Parameterized constraints — detection
        static constexpr bool is_foreign_key = (detail::is_foreign_key<Constraints>::value || ...);
        static constexpr bool has_default    = detail::extract_default<Constraints...>::found;
        static constexpr bool has_max_length = detail::extract_max_length<Constraints...>::found;
        static constexpr bool has_db_type    = detail::extract_db_type<Constraints...>::found;

        // Parameterized constraints — value access
        static constexpr std::string_view foreign_table() noexcept
            requires(detail::extract_foreign_key<Constraints...>::found)
        {
            return detail::extract_foreign_key<Constraints...>::table();
        }

        static constexpr std::string_view foreign_column() noexcept
            requires(detail::extract_foreign_key<Constraints...>::found)
        {
            return detail::extract_foreign_key<Constraints...>::column();
        }

        static constexpr std::string_view default_value() noexcept
            requires(detail::extract_default<Constraints...>::found)
        {
            return detail::extract_default<Constraints...>::value();
        }

        static constexpr std::size_t max_length() noexcept
            requires(detail::extract_max_length<Constraints...>::found)
        {
            return detail::extract_max_length<Constraints...>::value;
        }

        static constexpr std::string_view db_type() noexcept
            requires(detail::extract_db_type<Constraints...>::found)
        {
            return detail::extract_db_type<Constraints...>::value();
        }
    };

    template <typename T>
    concept IsStaticFieldSchema = requires {
        typename T::value_type;
        { T::name() } -> std::convertible_to<std::string_view>;
        { T::is_primary_key } -> std::convertible_to<bool>;
        { T::is_nullable } -> std::convertible_to<bool>;
    };

}  // namespace demiplane::db
