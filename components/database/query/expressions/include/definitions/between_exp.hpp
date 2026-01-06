#pragma once

#include <algorithm>

#include "../basic.hpp"

namespace demiplane::db {
    // todo: add constraint (concepts)
    template <typename Operand, typename Lower, typename Upper>
    class BetweenExpr : public Expression<BetweenExpr<Operand, Lower, Upper>> {
    public:
        template <typename OperandTp, typename LowerTp, typename UpperTp>
            requires std::constructible_from<Operand, OperandTp> && std::constructible_from<Lower, LowerTp> &&
                         std::constructible_from<Upper, UpperTp>
        constexpr BetweenExpr(OperandTp&& op, LowerTp&& l, UpperTp&& u) noexcept
            : operand_{std::forward<OperandTp>(op)},
              lower_{std::forward<LowerTp>(l)},
              upper_{std::forward<UpperTp>(u)} {
        }

        template <typename Self>
        [[nodiscard]] constexpr auto&& operand(this Self&& self) noexcept {
            return std::forward<Self>(self).operand_;
        }

        template <typename Self>
        [[nodiscard]] constexpr auto&& lower(this Self&& self) noexcept {
            return std::forward<Self>(self).lower_;
        }

        template <typename Self>
        [[nodiscard]] constexpr auto&& upper(this Self&& self) noexcept {
            return std::forward<Self>(self).upper_;
        }

    private:
        Operand operand_;
        Lower lower_;
        Upper upper_;
    };

    template <typename OperandTp, typename LowerBoundTp, typename UpperBoundTp>
    constexpr auto between(OperandTp&& operand, LowerBoundTp&& lower, UpperBoundTp&& upper) {
        auto wrapped_lower_bound = detail::make_literal_if_needed(std::forward<LowerBoundTp>(lower));
        auto wrapped_right_bound = detail::make_literal_if_needed(std::forward<UpperBoundTp>(upper));
        return BetweenExpr<std::remove_cvref_t<OperandTp>,
                           decltype(wrapped_lower_bound),
                           decltype(wrapped_right_bound)>{
            std::forward<OperandTp>(operand), std::move(wrapped_lower_bound), std::move(wrapped_right_bound)};
    }
}  // namespace demiplane::db
