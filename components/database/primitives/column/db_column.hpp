#pragma once
#include <string>
#include <string_view>
#include <utility>

#include <gears_concepts.hpp>
#include <gears_templates.hpp>

#include "db_typed_column.hpp"

namespace demiplane::db {

    class Column {
    public:
        template <gears::IsStringLike StringTp1, gears::IsStringLike StringTp2>
        constexpr explicit Column(StringTp1&& name, StringTp2&& table)
            : name_{std::forward<StringTp1>(name)},
              table_{std::forward<StringTp2>(table)} {
        }

        template <gears::IsStringLike StringTp>
        constexpr explicit Column(StringTp&& name)
            : name_{std::forward<StringTp>(name)} {
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
        constexpr Column& set_table(StringTp&& table) {
            table_ = std::forward<StringTp>(table);
            return *this;
        }

        template <gears::IsStringLike StringTp>
        constexpr Column& set_name(StringTp&& name) {
            name_ = std::forward<StringTp>(name);
            return *this;
        }

        template <gears::IsStringLike StringTp>
        [[nodiscard]] constexpr Column as(StringTp&& alias) const {
            Column result{name_, table_};
            result.alias_ = std::forward<StringTp>(alias);
            return result;
        }

        constexpr decltype(auto) accept(this auto&& self, auto& visitor);

    private:
        std::string name_;
        std::string table_{};
        std::string alias_{};
    };


    class AllColumns {
    public:
        template <gears::IsStringLike StringTp>
        constexpr explicit AllColumns(StringTp&& table)
            : table_{std::forward<StringTp>(table)} {
        }

        constexpr AllColumns() noexcept = default;

        [[nodiscard]] constexpr const std::string& table_name() const noexcept {
            return table_;
        }

        [[nodiscard]] constexpr Column as_column() const {
            return Column{"*", table_name()};
        }

        constexpr decltype(auto) accept(this auto&& self, auto& visitor);

    private:
        std::string table_;
    };


    template <typename CppType>
    [[nodiscard]] constexpr Column TypedColumn<CppType>::as_column() const {
        return Column{std::string{name_}, std::string{table_}};
    }

    // Column creation helpers
    constexpr Column col(const char* name) {
        return Column{std::string{name}};
    }

    constexpr Column col(const char* name, const char* table) {
        return Column{std::string{name}, std::string{table}};
    }

    constexpr AllColumns all(std::string table) {
        return AllColumns{std::move(table)};
    }

    constexpr AllColumns all() {
        return AllColumns{};
    }

    template <typename T>
    concept IsAllColumns = std::is_same_v<std::remove_cvref_t<T>, AllColumns>;

    template <typename T>
    concept IsColumn = std::is_same_v<std::remove_cvref_t<T>, Column>;

    template <typename T>
    concept IsColumnLike = (requires(const std::remove_cvref_t<T>& c) {
                               { c.name() } -> std::convertible_to<std::string_view>;
                               { c.table_name() } -> std::convertible_to<std::string_view>;
                               { c.alias() } -> std::convertible_to<std::string_view>;
                           }) || IsAllColumns<T>;


}  // namespace demiplane::db
