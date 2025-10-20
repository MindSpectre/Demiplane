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
        [[nodiscard]] constexpr auto&& operand(this Self&& self) {
            return std::forward<Self>(self).operand_;
        }

        template <typename Self>
        [[nodiscard]] constexpr auto&& lower(this Self&& self) {
            return std::forward<Self>(self).lower_;
        }

        template <typename Self>
        [[nodiscard]] constexpr auto&& upper(this Self&& self) {
            return std::forward<Self>(self).upper_;
        }

    private:
        Operand operand_;
        Lower lower_;
        Upper upper_;
    };

    template <typename Operand, typename LeftBound, typename UpperBound>
    constexpr auto between(Operand operand, LeftBound lower, UpperBound upper) {
        auto wrapped_lower_bound = detail::make_literal_if_needed(std::forward<LeftBound>(lower));
        auto wrapped_right_bound = detail::make_literal_if_needed(std::forward<UpperBound>(upper));
        return BetweenExpr<Operand, decltype(wrapped_lower_bound), decltype(wrapped_right_bound)>{
            std::move(operand), std::move(wrapped_lower_bound), std::move(wrapped_right_bound)};
    }
}  // namespace demiplane::db
