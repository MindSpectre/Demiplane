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
              upper_(std::move(u)) {
        }

        template <typename Self>
        [[nodiscard]] auto&& operand(this Self&& self) {
            return std::forward<Self>(self).operand_;
        }

        template <typename Self>
        [[nodiscard]] auto&& lower(this Self&& self) {
            return std::forward<Self>(self).lower_;
        }

        template <typename Self>
        [[nodiscard]] auto&& upper(this Self&& self) {
            return std::forward<Self>(self).upper_;
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
}  // namespace demiplane::db
