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
        [[nodiscard]] auto&& columns(this Self&& self) {
            return std::forward<Self>(self).columns_;
        }

        [[nodiscard]] bool distinct() const {
            return distinct_;
        }

        [[nodiscard]] auto from(TablePtr table) const {
            return FromTableExpr<SelectExpr>{*this, std::move(table)};
        }

        [[nodiscard]] auto from(std::string table_name) const {
            return FromTableExpr<SelectExpr>{*this, Table::make_ptr(std::move(table_name))};
        }

        [[nodiscard]] auto from(const Record& record) const {
            return FromTableExpr<SelectExpr>{*this, record.table_ptr()};
        }

        template <IsCteExpr Query>
        [[nodiscard]] auto from(Query&& query) const {
            return FromCteExpr<SelectExpr, Query>{*this, std::forward<Query>(query)};
        }

    private:
        std::tuple<Columns...> columns_;
        bool distinct_{false};
    };

    template <IsSelectable... Columns>
    constexpr auto select(Columns... columns) {
        return SelectExpr<Columns...>{std::move(columns)...};
    }

    template <IsSelectable... Columns>
    constexpr auto select_distinct(Columns... columns) {
        return SelectExpr<Columns...>{std::move(columns)...}.set_distinct(true);
    }

    inline auto select_from_schema(TablePtr schema) {
        return SelectExpr{all(schema->table_name())}.from(std::move(schema));
    }
}  // namespace demiplane::db
