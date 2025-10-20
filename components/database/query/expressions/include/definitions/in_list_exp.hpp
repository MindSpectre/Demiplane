#pragma once

#include <tuple>

#include "../basic.hpp"

namespace demiplane::db {
    template <typename Operand, typename... Values>
    class InListExpr : public Expression<InListExpr<Operand, Values...>> {
    public:
        constexpr explicit InListExpr(Operand op, Values... vals)
            : operand_(std::move(op)),
              values_(std::move(vals)...) {
        }

        template <typename Self>
        [[nodiscard]] constexpr auto&& operand(this Self&& self) {
            return std::forward<Self>(self).operand_;
        }

        template <typename Self>
        [[nodiscard]] constexpr auto&& values(this Self&& self) {
            return std::forward<Self>(self).values_;
        }

    private:
        Operand operand_;
        std::tuple<Values...> values_;
    };

    template <typename O, typename... Values>
    constexpr auto in(O operand, Values... values) {
        return InListExpr<O, decltype(detail::make_literal_if_needed(values))...>{
            std::move(operand), detail::make_literal_if_needed(std::move(values))...};
    }
}  // namespace demiplane::db
