#pragma once

#include <string_view>

#include <gears_types.hpp>

namespace demiplane::db {
    namespace constraints {
        // Simple tag constraints
        struct PrimaryKey {};
        struct NotNull {};
        struct Unique {};
        struct Indexed {};

        // Parameterized constraints
        template <gears::FixedString Table, gears::FixedString Column>
        struct ForeignKey {};

        template <gears::FixedString Value>
        struct Default {};

        template <std::size_t N>
        struct MaxLength {};

        template <gears::FixedString SqlType>
        struct DbType {};

    }  // namespace constraints

    namespace detail {


        template <typename T>
        struct is_foreign_key : std::false_type {};

        template <gears::FixedString T, gears::FixedString C>
        struct is_foreign_key<constraints::ForeignKey<T, C>> : std::true_type {};

        template <typename T>
        struct is_default : std::false_type {};

        template <gears::FixedString V>
        struct is_default<constraints::Default<V>> : std::true_type {};

        template <typename T>
        struct is_max_length : std::false_type {};

        template <std::size_t N>
        struct is_max_length<constraints::MaxLength<N>> : std::true_type {};

        template <typename T>
        struct is_db_type : std::false_type {};

        template <gears::FixedString S>
        struct is_db_type<constraints::DbType<S>> : std::true_type {};

        template <typename... Cs>
        struct extract_foreign_key {
            static constexpr bool found = false;
        };

        template <typename First, typename... Rest>
        struct extract_foreign_key<First, Rest...> : extract_foreign_key<Rest...> {};

        template <gears::FixedString Table, gears::FixedString Column, typename... Rest>
        struct extract_foreign_key<constraints::ForeignKey<Table, Column>, Rest...> {
            static constexpr bool found = true;
            static constexpr std::string_view table() noexcept {
                return Table.view();
            }
            static constexpr std::string_view column() noexcept {
                return Column.view();
            }
        };

        template <typename... Cs>
        struct extract_default {
            static constexpr bool found = false;
        };

        template <typename First, typename... Rest>
        struct extract_default<First, Rest...> : extract_default<Rest...> {};

        template <gears::FixedString Value, typename... Rest>
        struct extract_default<constraints::Default<Value>, Rest...> {
            static constexpr bool found = true;
            static constexpr std::string_view value() noexcept {
                return Value.view();
            }
        };

        template <typename... Cs>
        struct extract_max_length {
            static constexpr bool found = false;
        };

        template <typename First, typename... Rest>
        struct extract_max_length<First, Rest...> : extract_max_length<Rest...> {};

        template <std::size_t N, typename... Rest>
        struct extract_max_length<constraints::MaxLength<N>, Rest...> {
            static constexpr bool found        = true;
            static constexpr std::size_t value = N;
        };

        template <typename... Cs>
        struct extract_db_type {
            static constexpr bool found = false;
        };

        template <typename First, typename... Rest>
        struct extract_db_type<First, Rest...> : extract_db_type<Rest...> {};

        template <gears::FixedString SqlType, typename... Rest>
        struct extract_db_type<constraints::DbType<SqlType>, Rest...> {
            static constexpr bool found = true;
            static constexpr std::string_view value() noexcept {
                return SqlType.view();
            }
        };
    }  // namespace detail
}  // namespace demiplane::db
