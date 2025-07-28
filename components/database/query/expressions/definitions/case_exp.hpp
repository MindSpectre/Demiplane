#pragma once

#include "../basic.hpp"

namespace demiplane::db {
    template <typename... WhenClauses>
    class CaseExpr : public Expression<CaseExpr<WhenClauses...>> {
    public:
        [[nodiscard]] const std::tuple<WhenClauses...>& when_clauses() const {
            return when_clauses_;
        }

        // TODO: Add else clause

        constexpr explicit CaseExpr(WhenClauses... clauses)
            : when_clauses_(clauses...) {}
    private:
        std::tuple<WhenClauses...> when_clauses_;
    };
}