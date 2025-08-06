#pragma once

#include "../basic.hpp"

namespace demiplane::db {
    template <typename ConditionExpr, typename ValueExpr>
    struct WhenClause {
        ConditionExpr condition;
        ValueExpr value;

        constexpr WhenClause(ConditionExpr cond, ValueExpr val)
            : condition(std::move(cond)),
              value(std::move(val)) {}
    };

    template <typename... WhenClauses>
    class CaseExpr : public Expression<CaseExpr<WhenClauses...>> {
    public:
        [[nodiscard]] const std::tuple<WhenClauses...>& when_clauses() const {
            return when_clauses_;
        }

        template <typename ConditionExpr, typename ValueExpr>
        [[nodiscard]] auto when(ConditionExpr&& condition, ValueExpr&& value) const {
            using NewWhenClause = WhenClause<std::decay_t<ConditionExpr>, std::decay_t<ValueExpr>>;
            return CaseExpr<WhenClauses..., NewWhenClause>(
                std::tuple_cat(when_clauses_,
                               std::make_tuple(NewWhenClause(std::forward<ConditionExpr>(condition),
                                                             std::forward<ValueExpr>(value))))
            );
        }

        template <typename ElseExpr>
        [[nodiscard]] auto else_(ElseExpr&& else_expr) const {
            // Fixed: ElseExpr should be the template parameter, not in the variadic pack
            return CaseExprWithElse<std::decay_t<ElseExpr>, WhenClauses...>(
                when_clauses_, std::forward<ElseExpr>(else_expr)
            );
        }

        constexpr explicit CaseExpr(std::tuple<WhenClauses...> clauses)
            : when_clauses_(std::move(clauses)) {}

        constexpr explicit CaseExpr(WhenClauses... clauses)
            : when_clauses_(clauses...) {}

    private:
        std::tuple<WhenClauses...> when_clauses_;
    };

    template <typename ElseExpr, typename... WhenClauses>
    class CaseExprWithElse : public Expression<CaseExprWithElse<ElseExpr, WhenClauses...>> {
    public:
        [[nodiscard]] const std::tuple<WhenClauses...>& when_clauses() const {
            return when_clauses_;
        }

        [[nodiscard]] const ElseExpr& else_clause() const {
            return else_clause_;
        }

        constexpr CaseExprWithElse(const std::tuple<WhenClauses...>& when_clauses, ElseExpr else_expr)
            : when_clauses_(when_clauses),
              else_clause_(std::move(else_expr)) {}

    private:
        std::tuple<WhenClauses...> when_clauses_;
        ElseExpr else_clause_;
    };

    // Factory function to create initial CASE expression
    template <typename ConditionExpr, typename ValueExpr>
    [[nodiscard]] constexpr auto case_when(ConditionExpr&& condition, ValueExpr&& value) {
        using WhenType = WhenClause<std::decay_t<ConditionExpr>, std::decay_t<ValueExpr>>;
        return CaseExpr<WhenType>(WhenType(std::forward<ConditionExpr>(condition),
                                           std::forward<ValueExpr>(value)));
    }

    // Visitor support for CASE expressions
    template <typename Visitor>
    void visit_case_expression(Visitor& visitor, const auto& case_expr) {
        visitor.visit_case_start();

        // Visit each WHEN clause
        std::apply([&visitor](const auto&... when_clauses) {
            (..., (visitor.visit_when_start(),
                   when_clauses.condition.accept(visitor),
                   visitor.visit_then(),
                   when_clauses.value.accept(visitor),
                   visitor.visit_when_end()));
        }, case_expr.when_clauses());

        // Visit ELSE clause if present
        if constexpr (requires { case_expr.else_clause(); }) {
            visitor.visit_else_start();
            case_expr.else_clause().accept(visitor);
            visitor.visit_else_end();
        }

        visitor.visit_case_end();
    }
}
