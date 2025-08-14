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
        requires std::derived_from<std::remove_cvref_t<T>, Expression<std::remove_cvref_t<T>>>;
    };

    template <typename T>
    class Literal;

    template <typename T>
    concept IsLiteral = gears::is_specialization_of_v<std::remove_cvref_t<T>, Literal>;

    template <typename Left, typename Right, IsOperator Op>
    class BinaryExpr;

    template <typename T>
    concept IsBinaryOperator = gears::is_specialization_of_v<std::remove_cvref_t<T>, BinaryExpr>;

    template <typename Operand, IsOperator Op>
    class UnaryExpr;

    template <typename T>
    concept IsUnaryOperator = gears::is_specialization_of_v<std::remove_cvref_t<T>, UnaryExpr>;


    template <IsQuery Query>
    class ExistsExpr;

    template <typename T>
    concept IsExistExpr = gears::is_specialization_of_v<std::remove_cvref_t<T>, ExistsExpr>;

    // Between expression concept
    template <typename Operand, typename Lower, typename Upper>
    class BetweenExpr;

    template <typename T>
    concept IsBetweenExpr = gears::is_specialization_of_v<std::remove_cvref_t<T>, BetweenExpr>;

    // In list expression concept
    template <typename Operand, typename... Values>
    class InListExpr;

    template <typename T>
    concept IsInListExpr = gears::is_specialization_of_v<std::remove_cvref_t<T>, InListExpr>;

    template <typename T>
    concept IsCondition = IsBinaryOperator<T> ||
                          IsUnaryOperator<T> ||
                          IsExistExpr<T> ||
                          IsInListExpr<T> ||
                          IsBetweenExpr<T>;


    template <typename T>
    class CountExpr;

    template <typename T>
    concept IsCountExpr = gears::is_specialization_of_v<std::remove_cvref_t<T>, CountExpr>;

    template <typename T>
    class SumExpr;

    template <typename T>
    concept IsSumExpr = gears::is_specialization_of_v<std::remove_cvref_t<T>, SumExpr>;

    template <typename T>
    class AvgExpr;

    template <typename T>
    concept IsAvgExpr = gears::is_specialization_of_v<std::remove_cvref_t<T>, AvgExpr>;

    template <typename T>
    class MinExpr;

    template <typename T>
    concept IsMinExpr = gears::is_specialization_of_v<std::remove_cvref_t<T>, MinExpr>;

    template <typename T>
    class MaxExpr;

    template <typename T>
    concept IsMaxExpr = gears::is_specialization_of_v<std::remove_cvref_t<T>, MaxExpr>;

    template <typename T>
    concept IsAggregate = IsCountExpr<T> || IsSumExpr<T> || IsAvgExpr<T> || IsMaxExpr<T> || IsMinExpr<T>;

    // OrderBy expression concept
    template <typename T>
    class OrderBy;

    template <typename T>
    concept IsOrderBy = gears::is_specialization_of_v<std::remove_cvref_t<T>, OrderBy>;

    template <IsCondition ConditionExpr, typename ValueExpr>
    struct WhenClause;

    template <typename T>
    concept IsWhenClause = gears::is_specialization_of_v<std::remove_cvref_t<T>, WhenClause>;

    // Case expression concepts
    template <IsWhenClause... WhenClauses>
    class CaseExpr;

    template <typename ElseExpr, IsWhenClause... WhenClauses>
    class CaseExprWithElse;

    template <typename T>
    concept IsCaseExpr = gears::is_specialization_of_v<std::remove_cvref_t<T>, CaseExpr> ||
                         gears::is_specialization_of_v<std::remove_cvref_t<T>, CaseExprWithElse>;

    template <typename T>
    concept IsSelectable = IsColumn<T> || IsAggregate<T> || IsCaseExpr<T> || IsLiteral<T>;
    // Set operations concept
    template <typename Left, typename Right>
    class SetOpExpr;

    template <typename T>
    concept IsSetOpExpr = gears::is_specialization_of_v<std::remove_cvref_t<T>, SetOpExpr>;

    // Subquery concept
    template <IsQuery Query>
    class Subquery;

    template <typename T>
    concept IsSubquery = gears::is_specialization_of_v<std::remove_cvref_t<T>, Subquery>;

    // Limit expression concept
    template <IsQuery Query>
    class LimitExpr;

    template <typename T>
    concept IsLimitExpr = gears::is_specialization_of_v<std::remove_cvref_t<T>, LimitExpr>;

    // Group by expression concept
    template <IsQuery Query, IsColumn... GroupColumns>
    class GroupByColumnExpr;

    template <IsQuery PreGroupQuery, IsQuery GroupingCriteria>
    class GroupByQueryExpr;

    template <typename T>
    concept IsGroupByExpr = gears::is_specialization_of_v<std::remove_cvref_t<T>, GroupByColumnExpr> ||
                            gears::is_specialization_of_v<std::remove_cvref_t<T>, GroupByQueryExpr>;

    // Having expression concept
    template <IsQuery Query, IsCondition Condition>
    class HavingExpr;

    template <typename T>
    concept IsHavingExpr = gears::is_specialization_of_v<std::remove_cvref_t<T>, HavingExpr>;

    // Join expression concept
    template <IsQuery Query, IsCondition Condition>
    class JoinExpr;

    template <typename T>
    concept IsJoinExpr = gears::is_specialization_of_v<std::remove_cvref_t<T>, JoinExpr>;

    template <IsQuery Query>
    class CteExpr;

    template <typename T>
    concept IsCteExpr = gears::is_specialization_of_v<std::remove_cvref_t<T>, CteExpr>;

    template <IsQuery Query>
    class FromTableExpr;

    // From expression concept
    template <IsQuery Query, IsCteExpr CteExpr>
    class FromCteExpr;

    template <typename T>
    concept IsFromExpr = gears::is_specialization_of_v<std::remove_cvref_t<T>, FromTableExpr> ||
                         gears::is_specialization_of_v<std::remove_cvref_t<T>, FromCteExpr>;

    // CTE expression concept


    // Insert expression concepts
    class InsertExpr;

    template <typename T>
    concept IsInsertExpr = std::is_same_v<std::remove_cvref_t<T>, InsertExpr>;

    // Update expression concepts
    class UpdateExpr;

    template <IsCondition Condition>
    class UpdateWhereExpr;

    template <typename T>
    concept IsUpdateExpr = std::is_same_v<std::remove_cvref_t<T>, UpdateExpr> ||
                           gears::is_specialization_of_v<std::remove_cvref_t<T>, UpdateWhereExpr>;

    // Delete expression concepts
    class DeleteExpr;

    template <IsCondition Condition>
    class DeleteWhereExpr;

    template <typename T>
    concept IsDeleteExpr = std::is_same_v<std::remove_cvref_t<T>, DeleteExpr> ||
                           gears::is_specialization_of_v<std::remove_cvref_t<T>, DeleteWhereExpr>;

    template <IsQuery Query, IsCondition Condition>
    class WhereExpr;

    template <typename T>
    concept IsWhereExpr = gears::is_specialization_of_v<std::remove_cvref_t<T>, WhereExpr>;

    template <IsQuery Query, IsOrderBy... Orders>
    class OrderByExpr;

    template <typename T>
    concept IsOrderByExpr = gears::is_specialization_of_v<std::remove_cvref_t<T>, OrderByExpr>;

    template <IsSelectable... Columns>
    class SelectExpr;

    template <typename T>
    concept IsSelectExpr = gears::is_specialization_of_v<std::remove_cvref_t<T>, SelectExpr>;
}
