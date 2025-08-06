#pragma once

#include <algorithm>

#include "../basic.hpp"

namespace demiplane::db {
    template <IsQuery Query, typename... Columns>
    class GroupByExpr : public Expression<GroupByExpr<Query, Columns...>> {
    public:
        constexpr explicit GroupByExpr(Query q, Columns... cols)
            : query_(std::move(q)),
              columns_(cols...) {}

        // HAVING
        template <IsCondition Condition>
        constexpr auto having(Condition cond) const {
            return HavingExpr<GroupByExpr, Condition>{*this, std::move(cond)};
        }

        // ORDER BY (skip HAVING)
        template <typename... Orders>
        constexpr auto order_by(Orders... orders) const {
            return OrderByExpr<GroupByExpr, Orders...>{*this, orders...};
        }

        // LIMIT (skip HAVING)
        constexpr auto limit(std::size_t count) const {
            return LimitExpr<GroupByExpr>{*this, count, 0};
        }
        [[nodiscard]] const Query& query() const {
            return query_;
        }

        [[nodiscard]] const std::tuple<Columns...>& columns() const {
            return columns_;
        }
    private:
        Query query_;
        std::tuple<Columns...> columns_;
    };
}
