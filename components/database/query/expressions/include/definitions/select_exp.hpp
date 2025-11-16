#pragma once

#include <algorithm>

#include "../basic.hpp"

namespace demiplane::db {
    template <IsSelectable... Columns>
    class SelectExpr : public Expression<SelectExpr<Columns...>> {
    public:
        template <typename... ColumnsTp>
            requires (std::constructible_from<Columns, ColumnsTp> && ...)
        constexpr explicit SelectExpr(ColumnsTp&&... cols) noexcept
            : columns_{std::forward<ColumnsTp>(cols)...} {
        }

        template <typename Self>
        constexpr decltype(auto) set_distinct(this Self&& self, const bool d = true) noexcept {
            self.distinct_ = d;
            return std::forward<Self>(self);
        }

        template <typename Self>
        [[nodiscard]] constexpr auto&& columns(this Self&& self) noexcept {
            return std::forward<Self>(self).columns_;
        }

        [[nodiscard]] constexpr bool distinct() const noexcept {
            return distinct_;
        }


        template <typename Self, typename TableTp>
            requires std::constructible_from<TablePtr, std::remove_cvref_t<TableTp>>
        [[nodiscard]] constexpr auto from(this Self&& self, TableTp&& table) {
            return FromTableExpr<SelectExpr, TablePtr>{std::forward<Self>(self), std::forward<TableTp>(table)};
        }

        template <typename Self, typename TableTp>
            requires std::constructible_from<std::string, std::remove_cvref_t<TableTp>>
        [[nodiscard]] constexpr auto from(this Self&& self, TableTp&& table_name) {
            return FromTableExpr<SelectExpr, std::string>{std::forward<Self>(self), std::forward<TableTp>(table_name)};
        }

        template <typename Self>
        [[nodiscard]] constexpr auto from(this Self&& self, const char* table_name) {
            return FromTableExpr<SelectExpr, std::string_view>{std::forward<Self>(self), table_name};
        }

        template <typename Self, typename T>
            requires std::same_as<std::remove_cvref_t<T>, Record>
        [[nodiscard]] constexpr auto from(this Self&& self, T&& record) {
            return FromTableExpr<SelectExpr, TablePtr>{std::forward<Self>(self), std::forward<T>(record).table_ptr()};
        }

        template <typename Self, IsCteExpr Query>
        [[nodiscard]] constexpr auto from(this Self&& self, Query&& query) {
            return FromCteExpr<SelectExpr, std::remove_cvref_t<Query>>{std::forward<Self>(self),
                                                                       std::forward<Query>(query)};
        }

    private:
        std::tuple<Columns...> columns_;
        bool distinct_{false};
    };

    template <typename... ColumnsTp>
    constexpr auto select(ColumnsTp&&... columns) {
        return SelectExpr<decltype(detail::make_literal_if_needed(std::forward<ColumnsTp>(columns)))...>{
            detail::make_literal_if_needed(std::forward<ColumnsTp>(columns))...};
    }

    template <typename... ColumnsTp>
    constexpr auto select_distinct(ColumnsTp&&... columns) {
        return SelectExpr<decltype(detail::make_literal_if_needed(std::forward<ColumnsTp>(columns)))...>{
            detail::make_literal_if_needed(std::forward<ColumnsTp>(columns))...}
            .set_distinct(true);
    }

    template <typename TablePtrTp>
        requires std::constructible_from<TablePtr, TablePtrTp>
    constexpr auto select_from_schema(TablePtrTp&& schema) {
        return SelectExpr{all(schema->table_name())}.from(std::forward<TablePtrTp>(schema));
    }
}  // namespace demiplane::db
