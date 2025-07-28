#pragma once

#include <tuple>

#include "../basic.hpp"

namespace demiplane::db {
    template <typename Operand, typename... Values>
    class InListExpr : public Expression<InListExpr<Operand, Values...>> {
    public:
        constexpr explicit InListExpr(Operand op, Values... vals)
            : operand_(std::move(op)),
              values_(vals...) {}

        [[nodiscard]] const Operand& operand() const {
            return operand_;
        }

        [[nodiscard]] const std::tuple<Values...>& values() const {
            return values_;
        }

    private:
        Operand operand_;
        std::tuple<Values...> values_;
    };
    template <typename O, typename... Values>
    constexpr auto in(O operand, Values... values) {
        return InListExpr<O, Values...>{std::move(operand), values...};
    }
}