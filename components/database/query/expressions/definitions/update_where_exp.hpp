#pragma once

#include "../basic.hpp"
#include "update_exp.hpp"

namespace demiplane::db {

    template <IsTable TableT, IsCondition Condition>
    class UpdateWhereExpr : public Expression<UpdateWhereExpr<TableT, Condition>> {
    public:
        template <typename UpdateExprTp, typename ConditionTp>
            requires std::constructible_from<UpdateExpr<TableT>, UpdateExprTp> &&
                         std::constructible_from<Condition, ConditionTp>
        constexpr UpdateWhereExpr(UpdateExprTp&& u, ConditionTp&& c) noexcept
            : update_{std::forward<UpdateExprTp>(u)},
              condition_{std::forward<ConditionTp>(c)} {
        }

        template <typename Self>
        [[nodiscard]] constexpr auto&& update(this Self&& self) noexcept {
            return std::forward<Self>(self).update_;
        }

        template <typename Self>
        [[nodiscard]] constexpr auto&& condition(this Self&& self) noexcept {
            return std::forward<Self>(self).condition_;
        }

    private:
        UpdateExpr<TableT> update_;
        Condition condition_;
    };
}  // namespace demiplane::db
