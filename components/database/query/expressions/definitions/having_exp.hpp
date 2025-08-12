#pragma once

#include <algorithm>

#include "../basic.hpp"

namespace demiplane::db {

    template <IsQuery Query, IsCondition Condition>
    class HavingExpr : public Expression<HavingExpr<Query, Condition>> {
    public:
        constexpr HavingExpr(Query q, Condition c)
            : query_(std::move(q)),
              condition_(std::move(c)) {}

        template <IsOrderBy... Orders>
        constexpr auto order_by(Orders... orders) const {
            return OrderByExpr<HavingExpr, Orders...>{*this, orders...};
        }

        constexpr auto limit(std::size_t count) const {
            return LimitExpr<HavingExpr>{*this, count, 0};
        }

        template <typename Self>
        [[nodiscard]] auto&& query(this Self&& self) {
            return std::forward<Self>(self).query_;
        }

        template <typename Self>
        [[nodiscard]] auto&& condition(this Self&& self) {
            return std::forward<Self>(self).condition_;
        }

    private:
        Query query_;
        Condition condition_;
    };
}