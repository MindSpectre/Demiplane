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

        template <typename Self>
        [[nodiscard]] auto&& query(this Self&& self) {
            return std::forward<Self>(self).query_;
        }

        template <typename Self>
        [[nodiscard]] auto&& condition(this Self&& self) {
            return std::forward<Self>(self).condition_;
        }

        template <IsColumn... GroupColumns>
        constexpr auto group_by(GroupColumns... cols) const {
            return GroupByColumnExpr<WhereExpr, GroupColumns...>{*this, cols...};
        }

        template <IsOrderBy... Orders>
        constexpr auto order_by(Orders... orders) const {
            return OrderByExpr<WhereExpr, Orders...>{*this, orders...};
        }

        constexpr auto limit(const std::size_t count) const {
            return LimitExpr<WhereExpr>{*this, count, 0};
        }

    private:
        Query query_;
        Condition condition_;
    };
}
