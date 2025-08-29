#pragma once

#include <algorithm>

#include "../basic.hpp"
#include "delete_exp.hpp"

namespace demiplane::db {
    template <IsCondition Condition>
    class DeleteWhereExpr : public Expression<DeleteWhereExpr<Condition>> {
        public:
        DeleteWhereExpr(DeleteExpr d, Condition c)
            : del_(std::move(d)),
              condition_(std::move(c)) {
        }

        template <typename Self>
        [[nodiscard]] auto&& del(this Self&& self) {
            return std::forward<Self>(self).del_;
        }

        template <typename Self>
        [[nodiscard]] auto&& condition(this Self&& self) {
            return std::forward<Self>(self).condition_;
        }

        private:
        DeleteExpr del_;
        Condition condition_;
    };
}  // namespace demiplane::db
