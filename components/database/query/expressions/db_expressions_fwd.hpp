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

    template <typename Left, typename Right, IsOperator Op>
    class BinaryExpr;

    template <typename T>
    concept IsBinaryOperand = gears::derived_from_specialization_of_v<T, BinaryExpr>;

    template <IsBinaryOperand Operand, IsOperator Op>
    class UnaryExpr;

    template <typename T>
    concept IsUnaryOperand = gears::derived_from_specialization_of_v<T, UnaryExpr>;


    template <IsQuery Query>
    class ExistsExpr;

    template <typename T>
    concept IsExistExpr = gears::derived_from_specialization_of_v<T, ExistsExpr>;

    template <typename T>
    concept IsCondition = IsBinaryOperand<T> || IsUnaryOperand<T> || IsExistExpr<T>;

    template <typename T>
    concept IsScalar = gears::always_true_v<T>; // TODO


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

    template <typename T>
    concept IsSelectExpression = IsColumn<T> || IsAggregate<T> || IsScalar<T>;

    template <typename T>
    concept IsWhereExpression = IsColumn<T> || IsScalar<T>; // No aggregates!

    template <typename T>
    concept IsHavingExpression = IsAggregate<T> || IsScalar<T>; // Prefer aggregates

    template <typename T>
    concept IsOrderByExpression = IsColumn<T> || IsAggregate<T> || IsScalar<T>; // Context-dependent

    // OrderBy expression concept
    template <typename T>
    class OrderBy;

    template <typename T>
    concept IsOrderBy = gears::is_specialization_of_v<OrderBy, T>;

    // Case expression concepts
    template <typename... WhenClauses>
    class CaseExpr;

    template <typename ElseExpr, typename... WhenClauses>
    class CaseExprWithElse;

    template <typename ConditionExpr, typename ValueExpr>
    struct WhenClause;

    template <typename T>
    concept IsCaseExpr = gears::derived_from_specialization_of_v<T, CaseExpr> ||
                         gears::derived_from_specialization_of_v<T, CaseExprWithElse>;

    template <typename T>
    concept IsWhenClause = gears::is_specialization_of_v<WhenClause, T>;

    // Set operations concept
    template <typename Left, typename Right>
    class SetOpExpr;

    template <typename T>
    concept IsSetOp = gears::derived_from_specialization_of_v<T, SetOpExpr>;

    // Subquery concept
    template <IsQuery Query>
    class Subquery;

    template <typename T>
    concept IsSubquery = gears::derived_from_specialization_of_v<T, Subquery>;

    // Limit expression concept
    template <IsQuery Query>
    class LimitExpr;

    template <typename T>
    concept IsLimitExpr = gears::derived_from_specialization_of_v<T, LimitExpr>;

    // Group by expression concept
    template <IsQuery Query, typename... GroupColumns>
    class GroupByExpr;

    template <typename T>
    concept IsGroupByExpr = gears::derived_from_specialization_of_v<T, GroupByExpr>;

    // Having expression concept
    template <IsQuery Query, IsCondition Condition>
    class HavingExpr;

    template <typename T>
    concept IsHavingExpr = gears::derived_from_specialization_of_v<T, HavingExpr>;

    // Join expression concept
    template <IsQuery Query, typename JoinedTable, IsCondition Condition>
    class JoinExpr;

    template <typename T>
    concept IsJoinExpr = gears::derived_from_specialization_of_v<T, JoinExpr>;

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

    // From expression concept
    template <IsQuery Query>
    class FromExpr;

    template <typename T>
    concept IsFromExpr = gears::derived_from_specialization_of_v<T, FromExpr>;

    // CTE expression concept
    template <IsQuery Query>
    class CteExpr;

    template <typename T>
    concept IsCTEExpr = gears::derived_from_specialization_of_v<T, CteExpr>;

    // Insert expression concepts
    class InsertExpr;

    template <typename T>
    concept IsInsertExpr = std::is_same_v<T, InsertExpr>;

    // Update expression concepts
    class UpdateExpr;

    template <IsCondition Condition>
    class UpdateWhereExpr;

    template <typename T>
    concept IsUpdateExpr = std::is_same_v<T, UpdateExpr> ||
                           gears::derived_from_specialization_of_v<T, UpdateWhereExpr>;

    // Delete expression concepts
    class DeleteExpr;

    template <IsCondition Condition>
    class DeleteWhereExpr;

    template <typename T>
    concept IsDeleteExpr = std::is_same_v<T, DeleteExpr> ||
                           gears::derived_from_specialization_of_v<T, DeleteWhereExpr>;

    template <IsQuery Query, IsCondition Condition>
    class WhereExpr;

    template <IsQuery Query, IsOrderByExpression... Orders>
    class OrderByExpr;
}
