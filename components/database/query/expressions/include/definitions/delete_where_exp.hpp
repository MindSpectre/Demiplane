#pragma once

#include "../basic.hpp"
#include "delete_exp.hpp"

namespace demiplane::db {
    template <IsTable TableT, IsCondition Condition>
    class DeleteWhereExpr : public Expression<DeleteWhereExpr<TableT, Condition>> {
    public:
        template <typename DeleteExprTp, typename ConditionTp>
        constexpr DeleteWhereExpr(DeleteExprTp&& d, ConditionTp&& c)
            : del_{std::forward<DeleteExprTp>(d)},
              condition_{std::forward<ConditionTp>(c)} {
        }

        template <typename Self>
        [[nodiscard]] constexpr auto&& del(this Self&& self) {
            return std::forward<Self>(self).del_;
        }

        template <typename Self>
        [[nodiscard]] constexpr auto&& condition(this Self&& self) {
            return std::forward<Self>(self).condition_;
        }

    private:
        DeleteExpr<TableT> del_;
        Condition condition_;
    };
}  // namespace demiplane::db
