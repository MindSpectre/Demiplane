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

        [[nodiscard]] auto from(const Record& record) const {
            return FromTableExpr<SelectExpr>{*this, record.schema_ptr()};
        }

        [[nodiscard]] auto from(const std::string_view table_name) const {
            auto schema = std::make_shared<const TableSchema>(table_name);
            return FromTableExpr<SelectExpr>{*this, std::move(schema)};
        }

        template <IsQuery Query>
        [[nodiscard]] auto from(Query&& query) const {
            return FromQueryExpr<SelectExpr, Query>{*this, std::forward<Query>(query)};
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