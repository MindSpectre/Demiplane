#pragma once

#include <algorithm>

#include "../basic.hpp"

namespace demiplane::db {
    template <IsQuery Query, IsCondition Condition>
    class HavingExpr : public Expression<HavingExpr<Query, Condition>>,
                       public QueryOperations<HavingExpr<Query, Condition>, AllowOrderBy, AllowLimit> {
    public:
        template <typename QueryTp, typename ConditionTp>
            requires std::constructible_from<QueryTp, Query> && std::constructible_from<ConditionTp, Condition>
        constexpr HavingExpr(QueryTp&& q, ConditionTp&& c)
            : query_{std::forward<QueryTp>(q)},
              condition_{std::forward<ConditionTp>(c)} {
        }

        template <typename Self>
        [[nodiscard]] constexpr auto&& query(this Self&& self) noexcept {
            return std::forward<Self>(self).query_;
        }

        template <typename Self>
        [[nodiscard]] constexpr auto&& condition(this Self&& self) noexcept {
            return std::forward<Self>(self).condition_;
        }

    private:
        Query query_;
        Condition condition_;
    };
}  // namespace demiplane::db
