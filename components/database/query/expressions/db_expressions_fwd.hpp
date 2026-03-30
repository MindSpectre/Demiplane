#pragma once
#include <concepts>

#include <db_field_value.hpp>
#include <gears_templates.hpp>
#include <gears_utils.hpp>
namespace demiplane::db {

    // Minimal detector struct for duck-typed accept() checking.
    // Since QueryVisitor is now a CRTP template, we cannot forward-declare a
    // concrete type.  Instead we verify that accept() is callable with *any*
    // visitor-like argument.
    struct AcceptDetector {
        template <typename T>
        void visit(T&&) {
            gears::force_non_static(this);
            gears::force_non_const(this);
        }
    };

    template <typename T>
    concept HasAcceptVisitor = requires(std::remove_reference_t<T>& t, AcceptDetector& v) {
        { t.accept(v) };
    } || requires(const std::remove_reference_t<T>& t, AcceptDetector& v) {
        { t.accept(v) };
    };


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

    template <typename T = FieldValue>
    class Literal;

    template <typename T>
    concept IsLiteral = gears::is_specialization_of_v<std::remove_cvref_t<T>, Literal>;

    template <typename T>
    struct ParamPlaceholder;

    template <typename T>
    concept IsParamPlaceholder = gears::is_specialization_of_v<std::remove_cvref_t<T>, ParamPlaceholder>;

    template <typename Left, typename Right, IsOperator Op>
    class BinaryExpr;

    template <typename T>
    concept IsBinaryExpr = gears::is_specialization_of_v<std::remove_cvref_t<T>, BinaryExpr>;

    template <typename Operand, IsOperator Op>
    class UnaryExpr;

    template <typename T>
    concept IsUnaryExpr = gears::is_specialization_of_v<std::remove_cvref_t<T>, UnaryExpr>;


    template <IsQuery Query>
    class ExistsExpr;

    template <typename T>
    concept IsExistExpr = gears::is_specialization_of_v<std::remove_cvref_t<T>, ExistsExpr>;

    template <typename T>
    concept IsBetweenBound = HasAcceptVisitor<T> || IsFieldValueType<T>;

    // Between expression concept
    template <typename Operand, IsBetweenBound Lower, IsBetweenBound Upper>
    class BetweenExpr;

    template <typename T>
    concept IsBetweenExpr = gears::is_specialization_of_v<std::remove_cvref_t<T>, BetweenExpr>;

    // In list expression concept
    template <typename Operand, typename... Values>
    class InListExpr;

    template <typename T>
    concept IsInListExpr = gears::is_specialization_of_v<std::remove_cvref_t<T>, InListExpr>;

    template <typename T>
    concept IsCondition = IsBinaryExpr<T> || IsUnaryExpr<T> || IsExistExpr<T> || IsInListExpr<T> || IsBetweenExpr<T>;

    template <IsColumnLike ColT>
    class CountExpr;

    template <typename T>
    concept IsCountExpr = gears::is_specialization_of_v<std::remove_cvref_t<T>, CountExpr>;

    template <IsColumnLike ColT>
    class SumExpr;

    template <typename T>
    concept IsSumExpr = gears::is_specialization_of_v<std::remove_cvref_t<T>, SumExpr>;

    template <IsColumnLike ColT>
    class AvgExpr;

    template <typename T>
    concept IsAvgExpr = gears::is_specialization_of_v<std::remove_cvref_t<T>, AvgExpr>;

    template <IsColumnLike ColT>
    class MinExpr;

    template <typename T>
    concept IsMinExpr = gears::is_specialization_of_v<std::remove_cvref_t<T>, MinExpr>;

    template <IsColumnLike ColT>
    class MaxExpr;

    template <typename T>
    concept IsMaxExpr = gears::is_specialization_of_v<std::remove_cvref_t<T>, MaxExpr>;

    template <typename T>
    concept IsAggregate = IsCountExpr<T> || IsSumExpr<T> || IsAvgExpr<T> || IsMaxExpr<T> || IsMinExpr<T>;

    /**
     * @brief Concept to check if a type is a valid database operand for comparison/logical operators.
     *
     * This concept ensures that custom operators (==, !=, <, >, &&, ||, etc.) only apply
     * to database expression types and don't interfere with unrelated types (e.g., libpq types).
     * At least one operand must satisfy this concept for the operator overloads to be selected.
     */
    template <typename T>
    concept IsDbOperand = IsColumnLike<T> || IsLiteral<T> || IsParamPlaceholder<T> || IsCondition<T> || IsAggregate<T>;

    // OrderBy expression concept
    template <IsColumnLike ColT>
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
    concept IsSelectable = IsColumnLike<T> || IsAggregate<T> || IsCaseExpr<T> || IsLiteral<T>;
    // Set operations concept
    template <IsQuery Left, IsQuery Right>
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
    template <IsQuery Query, IsColumnLike... GroupColumns>
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
    template <IsQuery Query, IsCondition Condition, IsTable TableT>
    class JoinExpr;

    template <typename T>
    concept IsJoinExpr = gears::is_specialization_of_v<std::remove_cvref_t<T>, JoinExpr>;

    template <IsQuery Query>
    class CteExpr;

    template <typename T>
    concept IsCteExpr = gears::is_specialization_of_v<std::remove_cvref_t<T>, CteExpr>;

    template <IsQuery Query, IsTable TableT>
    class FromTableExpr;

    // From expression concept
    template <IsQuery Query, IsCteExpr CteExpr>
    class FromCteExpr;

    template <typename T>
    concept IsFromExpr = gears::is_specialization_of_v<std::remove_cvref_t<T>, FromTableExpr> ||
                         gears::is_specialization_of_v<std::remove_cvref_t<T>, FromCteExpr>;


    // Insert expression concepts
    template <IsTable TableT>
    class InsertExpr;

    template <typename T>
    concept IsInsertExpr = gears::is_specialization_of_v<std::remove_cvref_t<T>, InsertExpr>;

    // Update expression concepts
    template <IsTable TableT>
    class UpdateExpr;

    template <IsTable TableT, IsCondition Condition>
    class UpdateWhereExpr;

    template <typename T>
    concept IsUpdateExpr = gears::is_specialization_of_v<std::remove_cvref_t<T>, UpdateExpr> ||
                           gears::is_specialization_of_v<std::remove_cvref_t<T>, UpdateWhereExpr>;

    // Delete expression concepts
    template <IsTable TableT>
    class DeleteExpr;

    template <IsTable TableT, IsCondition Condition>
    class DeleteWhereExpr;

    template <typename T>
    concept IsDeleteExpr = gears::is_specialization_of_v<std::remove_cvref_t<T>, DeleteExpr> ||
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

    // DDL expression concepts
    template <IsTable TableT>
    class CreateTableExpr;

    template <typename T>
    concept IsCreateTableExpr = gears::is_specialization_of_v<std::remove_cvref_t<T>, CreateTableExpr>;

    template <IsTable TableT>
    class DropTableExpr;

    template <typename T>
    concept IsDropTableExpr = gears::is_specialization_of_v<std::remove_cvref_t<T>, DropTableExpr>;

    template <typename T>
    concept IsDdlExpr = IsCreateTableExpr<T> || IsDropTableExpr<T>;
}  // namespace demiplane::db
