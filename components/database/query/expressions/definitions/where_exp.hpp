#pragma once

#include <algorithm>

#include "../basic.hpp"

namespace demiplane::db {
    template <IsQuery Query, IsCondition Condition>
    class WhereExpr : public Expression<WhereExpr<Query, Condition>> {
    public:
        constexpr WhereExpr(Query q, Condition c)
            : query_(std::move(q)),
              condition_(std::move(c)) {}


        [[nodiscard]] const Query& query() const {
            return query_;
        }

        [[nodiscard]] const Condition& condition() const {
            return condition_;
        }


        // GROUP BY
        template <IsColumn... GroupColumns>
        constexpr auto group_by(GroupColumns... cols) const {
            return GroupByColumnExpr<WhereExpr, GroupColumns...>{*this, cols...};
        }

        // ORDER BY
        template <IsOrderBy... Orders>
        constexpr auto order_by(Orders... orders) const {
            return OrderByExpr<WhereExpr, Orders...>{*this, orders...};
        }

        // LIMIT
        constexpr auto limit(const std::size_t count) const {
            return LimitExpr<WhereExpr>{*this, count, 0};
        }

    private:
        Query query_;
        Condition condition_;
    };
}