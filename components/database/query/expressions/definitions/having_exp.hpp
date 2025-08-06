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

        // ORDER BY
        template <typename... Orders>
        constexpr auto order_by(Orders... orders) const {
            return OrderByExpr<HavingExpr, Orders...>{*this, orders...};
        }

        // LIMIT
        constexpr auto limit(std::size_t count) const {
            return LimitExpr<HavingExpr>{*this, count, 0};
        }

        [[nodiscard]] const Query& query() const {
            return query_;
        }

        [[nodiscard]] const Condition& condition() const {
            return condition_;
        }

    private:
        Query query_;
        Condition condition_;
    };
}
