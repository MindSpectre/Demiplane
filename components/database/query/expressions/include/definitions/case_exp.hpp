#pragma once

#include "../basic.hpp"

namespace demiplane::db {
    template <IsCondition ConditionExpr, typename ValueExpr>
    struct WhenClause {
        ConditionExpr condition;
        ValueExpr value;

        template <typename ConditionExprTp, typename ValueExprTp>
            requires std::constructible_from<ConditionExpr, ConditionExprTp>
                     && std::constructible_from<ValueExpr, ValueExprTp>
        constexpr WhenClause(ConditionExprTp&& cond, ValueExprTp&& val) noexcept
            : condition{std::forward<ConditionExprTp>(cond)},
              value{std::forward<ValueExprTp>(val)} {
        }
    };

    // Base class with common functionality
    template <IsCaseExpr Derived, IsWhenClause... WhenClauses>
    class CaseExprBase : public AliasableExpression<Derived> {
    public:
        template <typename TupleTp>
            requires std::constructible_from<std::tuple<WhenClauses...>, TupleTp>
        constexpr explicit CaseExprBase(TupleTp&& clauses) noexcept
            : when_clauses_{std::forward<TupleTp>(clauses)} {
        }

        template <typename... WhenClausesTp>
            requires (std::constructible_from<WhenClauses, WhenClausesTp> && ...)
        constexpr explicit CaseExprBase(WhenClausesTp&&... clauses) noexcept
            : when_clauses_{std::forward<WhenClausesTp>(clauses)...} {
        }

        template <typename Self>
        [[nodiscard]] constexpr auto&& when_clauses(this Self&& self) noexcept {
            return std::forward<Self>(self).when_clauses_;
        }

        template <typename ConditionExpr, typename ValueExpr>
        [[nodiscard]] constexpr auto when(ConditionExpr&& condition, ValueExpr&& value) const {
            auto wrapped_value = detail::make_literal_if_needed(std::forward<ValueExpr>(value));
            using NewWhenClause = WhenClause<std::decay_t<ConditionExpr>, decltype(wrapped_value)>;

            return static_cast<const Derived&>(*this).add_when_clause(
                NewWhenClause(std::forward<ConditionExpr>(condition), std::move(wrapped_value)));
        }

    protected:
        std::tuple<WhenClauses...> when_clauses_;
    };

    // Case expression without else clause
    template <IsWhenClause... WhenClauses>
    class CaseExpr : public CaseExprBase<CaseExpr<WhenClauses...>, WhenClauses...> {
        using Base = CaseExprBase<CaseExpr, WhenClauses...>;

    public:
        using Base::Base;  // Inherit constructors

        template <typename ElseExpr>
        [[nodiscard]] constexpr auto else_(ElseExpr&& else_expr) const {
            // Auto-wrap the else expression if needed
            auto wrapped_else = detail::make_literal_if_needed(std::forward<ElseExpr>(else_expr));
            return CaseExprWithElse<decltype(wrapped_else), WhenClauses...>(
                this->when_clauses_,
                std::move(wrapped_else));
        }

        // Used by base class when() method
        template <IsWhenClause NewWhenClause>
        [[nodiscard]] constexpr auto add_when_clause(NewWhenClause&& new_clause) const {
            return CaseExpr<WhenClauses..., std::decay_t<NewWhenClause>>(
                std::tuple_cat(this->when_clauses_, std::make_tuple(std::forward<NewWhenClause>(new_clause))));
        }
    };

    // Case expression with else clause
    template <typename ElseExpr, IsWhenClause... WhenClauses>
    class CaseExprWithElse : public CaseExprBase<CaseExprWithElse<ElseExpr, WhenClauses...>, WhenClauses...> {
        using Base = CaseExprBase<CaseExprWithElse, WhenClauses...>;

    public:
        template <typename TupleTp, typename ElseExprTp>
            requires std::constructible_from<std::tuple<WhenClauses...>, TupleTp>
                     && std::constructible_from<ElseExpr, ElseExprTp>
        constexpr CaseExprWithElse(TupleTp&& when_clauses, ElseExprTp&& else_expr) noexcept
            : Base{std::forward<TupleTp>(when_clauses)},
              else_clause_{std::forward<ElseExprTp>(else_expr)} {
        }

        template <typename Self>
        [[nodiscard]] constexpr auto&& else_clause(this Self&& self) noexcept {
            return std::forward<Self>(self).else_clause_;
        }

        // Used by base class when() method
        template <IsWhenClause NewWhenClause>
        [[nodiscard]] constexpr auto add_when_clause(NewWhenClause&& new_clause) const {
            return CaseExprWithElse<ElseExpr, WhenClauses..., std::decay_t<NewWhenClause>>(
                std::tuple_cat(this->when_clauses_, std::make_tuple(std::forward<NewWhenClause>(new_clause))),
                else_clause_);
        }

    private:
        ElseExpr else_clause_;
    };

    // Factory function
    template <IsCondition ConditionExpr, typename ValueExpr>
    [[nodiscard]] constexpr auto case_when(ConditionExpr&& condition, ValueExpr&& value) {
        // Auto-wrap the value if needed
        auto wrapped_value = detail::make_literal_if_needed(std::forward<ValueExpr>(value));
        using WhenType = WhenClause<std::decay_t<ConditionExpr>, decltype(wrapped_value)>;

        return CaseExpr<WhenType>(
            WhenType(std::forward<ConditionExpr>(condition), std::move(wrapped_value)));
    }

}  // namespace demiplane::db
