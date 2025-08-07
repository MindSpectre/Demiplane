#pragma once
#include <concepts>

#include <db_core_fwd.hpp>
#include <gears_templates.hpp>

namespace demiplane::db {
    class QueryVisitor;
    struct OpBase;

    template <typename T>
    concept IsOperator = std::is_base_of_v<OpBase, T>;

    template <class Derived>
    class Expression;
    template <typename T>
    concept IsQuery = requires {
        // Must inherit from Expression (CRTP pattern)
        requires std::derived_from<T, Expression<T>>;
    };

    template <typename T>
    struct Literal;

    template <typename T>
    concept IsLiteral = gears::is_specialization_of_v<Literal, T>;

    template <typename Left, typename Right, IsOperator Op>
    class BinaryExpr;

    template <typename T>
    concept IsBinaryOperand = gears::derived_from_specialization_of_v<T, BinaryExpr>;

    template <typename Operand, IsOperator Op>
    class UnaryExpr;

    template <typename T>
    concept IsUnaryOperand = gears::derived_from_specialization_of_v<T, UnaryExpr>;


    template <IsQuery Query>
    class ExistsExpr;

    template <typename T>
    concept IsExistExpr = gears::derived_from_specialization_of_v<T, ExistsExpr>;

    // Between expression concept
    template <typename Operand, typename Lower, typename Upper>
    class BetweenExpr;

    template <typename T>
    concept IsBetweenExpr = gears::derived_from_specialization_of_v<T, BetweenExpr>;

    // In list expression concept
    template <typename Operand, typename... Values>
    class InListExpr;

    template <typename T>
    concept IsInListExpr = gears::derived_from_specialization_of_v<T, InListExpr>;

    template <typename T>
    concept IsCondition = IsBinaryOperand<T> ||
                          IsUnaryOperand<T> ||
                          IsExistExpr<T> ||
                          IsInListExpr<T> ||
                          IsBetweenExpr<T>;


    template <typename T>
    class CountExpr;

    template <typename T>
    class SumExpr;

    template <typename T>
    class AvgExpr;

    template <typename T>
    class MinExpr;

    template <typename T>
    class MaxExpr;


    template <typename T>
    concept IsAggregate = demiplane::gears::is_specialization_of_v<CountExpr, T> ||
                          demiplane::gears::is_specialization_of_v<SumExpr, T> ||
                          demiplane::gears::is_specialization_of_v<AvgExpr, T> ||
                          demiplane::gears::is_specialization_of_v<MaxExpr, T> ||
                          demiplane::gears::is_specialization_of_v<MinExpr, T>;

    // OrderBy expression concept
    template <typename T>
    class OrderBy;

    template <typename T>
    concept IsOrderBy = gears::is_specialization_of_v<OrderBy, T>;

    template <IsCondition ConditionExpr, typename ValueExpr>
    struct WhenClause;

    template <typename T>
    concept IsWhenClause = gears::is_specialization_of_v<WhenClause, T>;

    // Case expression concepts
    template <IsWhenClause... WhenClauses>
    class CaseExpr;

    template <typename ElseExpr, IsWhenClause... WhenClauses>
    class CaseExprWithElse;

    template <typename T>
    concept IsCaseExpr = gears::derived_from_specialization_of_v<T, CaseExpr> ||
                         gears::derived_from_specialization_of_v<T, CaseExprWithElse>;

    template <typename T>
    concept IsSelectable = IsColumn<T> || IsAggregate<T> || IsCaseExpr<T> || IsLiteral<T>;
    // Set operations concept
    template <typename Left, typename Right>
    class SetOpExpr;

    // Subquery concept
    template <IsQuery Query>
    class Subquery;

    // Limit expression concept
    template <IsQuery Query>
    class LimitExpr;

    // Group by expression concept
    template <IsQuery Query, IsColumn... GroupColumns>
    class GroupByExpr;

    // Having expression concept
    template <IsQuery Query, IsCondition Condition>
    class HavingExpr;

    // Join expression concept
    template <IsQuery Query, IsTableSchema JoinedTable, IsCondition Condition>
    class JoinExpr;

    // From expression concept
    template <IsQuery Query>
    class FromExpr;

    // CTE expression concept
    template <IsQuery Query>
    class CteExpr;

    // Insert expression concepts
    class InsertExpr;

    template <typename T>
    concept IsInsertExpr = std::is_same_v<T, InsertExpr>;

    // Update expression concepts
    class UpdateExpr;

    template <IsCondition Condition>
    class UpdateWhereExpr;

    // Delete expression concepts
    class DeleteExpr;

    template <IsCondition Condition>
    class DeleteWhereExpr;

    template <IsQuery Query, IsCondition Condition>
    class WhereExpr;

    template <IsQuery Query, IsOrderBy... Orders>
    class OrderByExpr;
}
