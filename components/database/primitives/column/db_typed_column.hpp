#pragma once

#include <string>
#include <string_view>

#include <gears_concepts.hpp>

namespace demiplane::db {

    class Column;

    /**
     * @brief Column with a compile-time C++ type tag
     *
     * Like Column, but carries `value_type` for type-safe parameter binding
     * and compile-time type assertions. Produced by StaticTable::column<>().
     *
     * @tparam CppType  The C++ type this column maps to (int, std::string, bool, etc.)
     */
    template <typename CppType>
    class TypedColumn {
    public:
        using value_type = CppType;

        template <gears::IsStringLike StringTp1, gears::IsStringLike StringTp2>
        constexpr TypedColumn(StringTp1&& name, StringTp2&& table)
            : name_{std::forward<StringTp1>(name)},
              table_{std::forward<StringTp2>(table)} {
        }

        template <gears::IsStringLike StringTp1, gears::IsStringLike StringTp2, gears::IsStringLike StringTp3>
        constexpr TypedColumn(StringTp1&& name, StringTp2&& table, StringTp3&& alias)
            : name_{std::forward<StringTp1>(name)},
              table_{std::forward<StringTp2>(table)},
              alias_{std::forward<StringTp3>(alias)} {
        }

        [[nodiscard]] constexpr std::string_view name() const noexcept {
            return name_;
        }

        [[nodiscard]] constexpr std::string_view table_name() const noexcept {
            return table_;
        }

        [[nodiscard]] constexpr std::string_view alias() const noexcept {
            return alias_;
        }

        template <gears::IsStringLike StringTp>
        [[nodiscard]] constexpr TypedColumn as(StringTp&& alias) const {
            return TypedColumn{name_, table_, std::forward<StringTp>(alias)};
        }

        [[nodiscard]] constexpr Column as_column() const;

        constexpr decltype(auto) accept(this auto&& self, auto& visitor);

    private:
        std::string name_;
        std::string table_;
        std::string alias_;
    };

    template <typename T>
    concept IsTypedColumn = requires { typename T::value_type; } && requires(const T& t) {
        { t.name() } -> std::convertible_to<std::string_view>;
        { t.table_name() } -> std::convertible_to<std::string_view>;
        { t.alias() } -> std::convertible_to<std::string_view>;
    } && !std::is_same_v<std::remove_cvref_t<T>, Column>;

}  // namespace demiplane::db
