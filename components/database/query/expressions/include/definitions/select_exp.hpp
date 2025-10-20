#pragma once

#include <algorithm>

#include "../basic.hpp"

namespace demiplane::db {
    template <IsSelectable... Columns>
    class SelectExpr : public Expression<SelectExpr<Columns...>> {
    public:
        constexpr explicit SelectExpr(Columns... cols)
            : columns_(std::move(cols)...) {
        }

        constexpr SelectExpr& set_distinct(const bool d = true) {
            distinct_ = d;
            return *this;
        }

        template <typename Self>
        [[nodiscard]] constexpr auto&& columns(this Self&& self) {
            return std::forward<Self>(self).columns_;
        }

        [[nodiscard]] constexpr bool distinct() const {
            return distinct_;
        }

        [[nodiscard]] constexpr auto from(TablePtr table) const {
            return FromTableExpr<SelectExpr>{*this, std::move(table)};
        }

        [[nodiscard]] constexpr auto from(std::string table_name) const {
            return FromTableExpr<SelectExpr>{*this, Table::make_ptr(std::move(table_name))};
        }

        [[nodiscard]] constexpr auto from(const Record& record) const {
            return FromTableExpr<SelectExpr>{*this, record.table_ptr()};
        }

        template <IsCteExpr Query>
        [[nodiscard]] constexpr auto from(Query&& query) const {
            return FromCteExpr<SelectExpr, Query>{*this, std::forward<Query>(query)};
        }

    private:
        std::tuple<Columns...> columns_;
        bool distinct_{false};
    };

    template <typename... Columns>
    constexpr auto select(Columns... columns) {
        return SelectExpr<decltype(detail::make_literal_if_needed(columns))...>{
            detail::make_literal_if_needed(std::move(columns))...};
    }

    template <typename... Columns>
    constexpr auto select_distinct(Columns... columns) {
        return SelectExpr<decltype(detail::make_literal_if_needed(columns))...>{
            detail::make_literal_if_needed(std::move(columns))...}
            .set_distinct(true);
    }

    inline auto select_from_schema(TablePtr schema) {
        return SelectExpr{all(schema->table_name())}.from(std::move(schema));
    }
}  // namespace demiplane::db
