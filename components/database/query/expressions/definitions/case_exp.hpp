#pragma once

#include "../basic.hpp"

namespace demiplane::db {
    template <IsCondition ConditionExpr, typename ValueExpr>
    struct WhenClause {
        ConditionExpr condition;
        ValueExpr value;

        constexpr WhenClause(ConditionExpr cond, ValueExpr val)
            : condition(std::move(cond)),
              value(std::move(val)) {}
    };

    // Base class with common functionality
    template <typename Derived, IsWhenClause... WhenClauses>
    class CaseExprBase : public AliasableExpression<Derived> {
    public:
        constexpr explicit CaseExprBase(std::tuple<WhenClauses...> clauses)
            : when_clauses_(std::move(clauses)) {}

        constexpr explicit CaseExprBase(WhenClauses... clauses)
            : when_clauses_(clauses...) {}

        [[nodiscard]] const std::tuple<WhenClauses...>& when_clauses() const {
            return when_clauses_;
        }

        template <typename ConditionExpr, typename ValueExpr>
        [[nodiscard]] auto when(ConditionExpr&& condition, ValueExpr&& value) const {
            using NewWhenClause = WhenClause<std::decay_t<ConditionExpr>, std::decay_t<ValueExpr>>;
            return static_cast<const Derived&>(*this).add_when_clause(
                NewWhenClause(std::forward<ConditionExpr>(condition),
                              std::forward<ValueExpr>(value))
            );
        }

    protected:
        std::tuple<WhenClauses...> when_clauses_;
    };

    // Case expression without else clause
    template <IsWhenClause... WhenClauses>
    class CaseExpr : public CaseExprBase<CaseExpr<WhenClauses...>, WhenClauses...> {
        using Base = CaseExprBase<CaseExpr, WhenClauses...>;

    public:
        using Base::Base; // Inherit constructors

        template <typename ElseExpr>
        [[nodiscard]] auto else_(ElseExpr&& else_expr) const {
            return CaseExprWithElse<std::decay_t<ElseExpr>, WhenClauses...>(
                this->when_clauses_, std::forward<ElseExpr>(else_expr)
            );
        }

        // Used by base class when() method
        template <typename NewWhenClause>
        [[nodiscard]] auto add_when_clause(NewWhenClause&& new_clause) const {
            return CaseExpr<WhenClauses..., std::decay_t<NewWhenClause>>(
                std::tuple_cat(this->when_clauses_, std::make_tuple(std::forward<NewWhenClause>(new_clause)))
            );
        }
    };

    // Case expression with else clause
    template <typename ElseExpr, IsWhenClause... WhenClauses>
    class CaseExprWithElse : public CaseExprBase<CaseExprWithElse<ElseExpr, WhenClauses...>, WhenClauses...> {
        using Base = CaseExprBase<CaseExprWithElse, WhenClauses...>;

    public:
        constexpr CaseExprWithElse(const std::tuple<WhenClauses...>& when_clauses, ElseExpr else_expr)
            : Base(when_clauses),
              else_clause_(std::move(else_expr)) {}

        [[nodiscard]] const ElseExpr& else_clause() const {
            return else_clause_;
        }

        // Used by base class when() method
        template <typename NewWhenClause>
        [[nodiscard]] auto add_when_clause(NewWhenClause&& new_clause) const {
            return CaseExprWithElse<ElseExpr, WhenClauses..., std::decay_t<NewWhenClause>>(
                std::tuple_cat(this->when_clauses_, std::make_tuple(std::forward<NewWhenClause>(new_clause))),
                else_clause_
            );
        }

    private:
        ElseExpr else_clause_;
    };

    // Factory function
    template <IsCondition ConditionExpr, typename ValueExpr>
    [[nodiscard]] constexpr auto case_when(ConditionExpr&& condition, ValueExpr&& value) {
        using WhenType = WhenClause<std::decay_t<ConditionExpr>, std::decay_t<ValueExpr>>;
        return CaseExpr<WhenType>(
            WhenType(std::forward<ConditionExpr>(condition), std::forward<ValueExpr>(value))
        );
    }

}
