#pragma once

#include "../basic.hpp"

namespace demiplane::db {
    template <IsQuery Query, IsCondition Condition>
    class WhereExpr : public Expression<WhereExpr<Query, Condition>>,
                      public QueryOperations<WhereExpr<Query, Condition>, AllowGroupBy, AllowOrderBy, AllowLimit> {
    public:
        template <typename QueryTp, typename ConditionTp>
            requires std::constructible_from<Query, QueryTp> && std::constructible_from<Condition, ConditionTp>
        constexpr WhereExpr(QueryTp&& q, ConditionTp&& c) noexcept
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
