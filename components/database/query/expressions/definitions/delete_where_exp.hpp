#pragma once

#include <algorithm>

#include "delete_exp.hpp"
#include "../basic.hpp"

namespace demiplane::db {
    template <IsCondition Condition>
    class DeleteWhereExpr : public Expression<DeleteWhereExpr<Condition>> {
    public:
        DeleteWhereExpr(DeleteExpr d, Condition c)
            : del_(std::move(d)),
              condition_(std::move(c)) {}

        [[nodiscard]] const DeleteExpr& del() const {
            return del_;
        }

        [[nodiscard]] const Condition& condition() const {
            return condition_;
        }

    private:
        DeleteExpr del_;
        Condition condition_;
    };
}
