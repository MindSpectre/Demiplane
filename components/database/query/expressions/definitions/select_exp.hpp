#pragma once

#include <algorithm>

#include "db_column.hpp"
#include "db_record.hpp"
#include "db_table_schema.hpp"
#include "../basic.hpp"

namespace demiplane::db {
    template <IsSelectable... Columns>
    class SelectExpr : public Expression<SelectExpr<Columns...>> {
    public:
        constexpr explicit SelectExpr(Columns... cols)
            : columns_(std::forward<Columns>(cols)...) {}

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

        [[nodiscard]] auto from(TableSchemaPtr table) const {
            return FromTableExpr<SelectExpr>{*this, std::move(table)};
        }

        [[nodiscard]] auto from(std::string table_name) const {
            return FromTableExpr<SelectExpr>{*this, TableSchema::make_ptr(std::move(table_name))};
        }

        [[nodiscard]] auto from(const Record& record) const {
            return FromTableExpr<SelectExpr>{*this, record.schema_ptr()};
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
        return SelectExpr<Columns...>{columns...};
    }

    template <IsSelectable... Columns>
    constexpr auto select_distinct(Columns... columns) {
        return SelectExpr<Columns...>{columns...}.set_distinct(true);
    }

    inline auto select_from_schema(TableSchemaPtr schema) {
        return SelectExpr{all(schema->table_name())}.from(std::move(schema));
    }
}
