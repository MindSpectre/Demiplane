#pragma once

#include <algorithm>
#include <utility>

#include "db_table_schema.hpp"
#include "group_by_exp.hpp"
#include "../basic.hpp"

namespace demiplane::db {
    template <IsQuery Select>
    class FromTableExpr : public AliasableExpression<FromTableExpr<Select>>,
                          public QueryOperations<FromTableExpr<Select>,
                                                 AllowGroupBy, AllowOrderBy, AllowLimit,
                                                 AllowJoin, AllowWhere> {
    public:
        constexpr FromTableExpr(Select select_q, TableSchemaPtr table)
            : select_(std::move(select_q)),
              table_(std::move(table)) {}

        template <typename Self>
        [[nodiscard]] auto&& select(this Self&& self) {
            return std::forward<Self>(self).select_;
        }

        template <typename Self>
        [[nodiscard]] auto&& table(this Self&& self) {
            return std::forward<Self>(self).table_;
        }

    private:
        Select select_;
        TableSchemaPtr table_;
    };

    // FromQueryExpr with proper inheritance order
    template <IsQuery Select, IsQuery FromAnotherQuery>
    class FromQueryExpr : public Expression<FromQueryExpr<Select, FromAnotherQuery>>,
                          public QueryOperations<FromQueryExpr<Select, FromAnotherQuery>,
                                                 AllowGroupBy, AllowOrderBy, AllowLimit,
                                                 AllowJoin, AllowWhere> {
    public:
        constexpr FromQueryExpr(Select select_q, FromAnotherQuery&& expr)
            : select_(std::move(select_q)),
              query_(std::forward<FromAnotherQuery>(expr)) {}

        template <typename Self>
        [[nodiscard]] auto&& select(this Self&& self) {
            return std::forward<Self>(self).select_;
        }

        template <typename Self>
        [[nodiscard]] auto&& query(this Self&& self) {
            return std::forward<Self>(self).query_;
        }

    private:
        Select select_;
        FromAnotherQuery query_;
    };
}
