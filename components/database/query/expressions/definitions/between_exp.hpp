#pragma once

#include <algorithm>

#include "../basic.hpp"

namespace demiplane::db {
    template <typename Operand, typename Lower, typename Upper>
    class BetweenExpr : public Expression<BetweenExpr<Operand, Lower, Upper>> {
    public:
        constexpr BetweenExpr(Operand op, Lower l, Upper u)
            : operand_(std::move(op)),
              lower_(std::move(l)),
              upper_(std::move(u)) {}

        [[nodiscard]] const Operand& operand() const {
            return operand_;
        }

        [[nodiscard]] const Lower& lower() const {
            return lower_;
        }

        [[nodiscard]] const Upper& upper() const {
            return upper_;
        }

    private:
        Operand operand_;
        Lower lower_;
        Upper upper_;
    };
    template <typename O, typename L, typename U>
    constexpr auto between(O operand, L lower, U upper) {
        return BetweenExpr<O, L, U>{std::move(operand), std::move(lower), std::move(upper)};
    }
}