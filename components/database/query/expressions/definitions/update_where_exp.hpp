#pragma once

#include <algorithm>
#include "update_exp.hpp"
#include "../basic.hpp"

namespace demiplane::db {
    template <IsCondition Condition>
    class UpdateWhereExpr : public Expression<UpdateWhereExpr<Condition>> {
    public:
        UpdateWhereExpr(UpdateExpr u, Condition c)
            : update_(std::move(u)),
              condition_(std::move(c)) {}

        template <typename Self>
        [[nodiscard]] auto&& update(this Self&& self) {
            return std::forward<Self>(self).update_;
        }


        template <typename Self>
        [[nodiscard]] auto&& condition(this Self&& self) {
            return std::forward<Self>(self).condition_;
        }

    private:
        UpdateExpr update_;
        Condition condition_;
    };
}
