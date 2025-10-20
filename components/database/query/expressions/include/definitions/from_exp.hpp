#pragma once

#include <algorithm>
#include <utility>

#include "../basic.hpp"

namespace demiplane::db {
    template <IsQuery Select>
    class FromTableExpr
        : public AliasableExpression<FromTableExpr<Select>>,
          public QueryOperations<FromTableExpr<Select>, AllowGroupBy, AllowOrderBy, AllowLimit, AllowJoin, AllowWhere> {
    public:
        constexpr FromTableExpr(Select select_q, TablePtr table)
            : select_(std::move(select_q)),
              table_(std::move(table)) {
        }

        template <typename Self>
        [[nodiscard]] auto&& select(this Self&& self) {
            return std::forward<Self>(self).select_;
        }

        [[nodiscard]] constexpr const TablePtr& table() const {
            return table_;
        }

    private:
        Select select_;
        TablePtr table_;
    };

    // FromQueryExpr with proper inheritance order
    template <IsQuery Select, IsCteExpr CteQuery>
    class FromCteExpr : public Expression<FromCteExpr<Select, CteQuery>>,
                        public QueryOperations<FromCteExpr<Select, CteQuery>,
                                               AllowGroupBy,
                                               AllowOrderBy,
                                               AllowLimit,
                                               AllowJoin,
                                               AllowWhere> {
    public:
        constexpr FromCteExpr(Select select_q, CteQuery&& expr)
            : select_(std::move(select_q)),
              query_(std::forward<CteQuery>(expr)) {
        }

        template <typename Self>
        [[nodiscard]] constexpr auto&& select(this Self&& self) {
            return std::forward<Self>(self).select_;
        }

        template <typename Self>
        [[nodiscard]] constexpr auto&& cte_query(this Self&& self) {
            return std::forward<Self>(self).query_;
        }

    private:
        Select select_;
        CteQuery query_;
    };
}  // namespace demiplane::db
