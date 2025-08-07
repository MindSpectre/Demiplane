#pragma once

#include <algorithm>

#include "../basic.hpp"
namespace demiplane::db {

    template <IsCondition Condition>
    class UpdateWhereExpr : public Expression<UpdateWhereExpr<Condition>> {
    public:
        UpdateWhereExpr(UpdateExpr u, Condition c)
            : update_(std::move(u)),
              condition_(std::move(c)) {}

        [[nodiscard]] const UpdateExpr& update() const {
            return update_;
        }

        void set_update(UpdateExpr update) {
            update_ = std::move(update);
        }

        [[nodiscard]] const Condition& condition() const {
            return condition_;
        }

        void set_condition(Condition condition) {
            condition_ = std::move(condition);
        }

    private:
        UpdateExpr update_;
        Condition condition_;
    };

    // UPDATE builder function
}
