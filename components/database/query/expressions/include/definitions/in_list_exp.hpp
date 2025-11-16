#pragma once

#include <tuple>

#include "../basic.hpp"

namespace demiplane::db {
    template <typename Operand, typename... Values>
    class InListExpr : public Expression<InListExpr<Operand, Values...>> {
    public:
        template <typename OperandTp, typename... ValuesTp>
            requires std::constructible_from<Operand, OperandTp> && (std::constructible_from<Values, ValuesTp> && ...)
        constexpr explicit InListExpr(OperandTp&& op, ValuesTp&&... vals) noexcept
            : operand_{std::forward<OperandTp>(op)},
              values_{std::forward<ValuesTp>(vals)...} {
        }

        template <typename Self>
        [[nodiscard]] constexpr auto&& operand(this Self&& self) noexcept {
            return std::forward<Self>(self).operand_;
        }

        template <typename Self>
        [[nodiscard]] constexpr auto&& values(this Self&& self) noexcept {
            return std::forward<Self>(self).values_;
        }

    private:
        Operand operand_;
        std::tuple<Values...> values_;
    };

    template <typename OperandTp, typename... ValuesTp>
    constexpr auto in(OperandTp&& operand, ValuesTp&&... values) {
        return InListExpr<std::remove_cvref_t<OperandTp>, decltype(detail::make_literal_if_needed(values))...>{
            std::forward<OperandTp>(operand), detail::make_literal_if_needed(std::forward<ValuesTp>(values))...};
    }
}  // namespace demiplane::db
