#pragma once

#include <concepts>

namespace demiplane::test {

    struct QueryTag {};

    template <typename T>
    concept IsQueryTag = std::derived_from<T, QueryTag>;

    // SELECT query tags - mirrors db_select_queries_test.cpp
    namespace sel {
        struct BasicSelect : QueryTag {};          // BasicSelectExpression
        struct SelectAllColumns : QueryTag {};     // SelectAllColumnsExpression
        struct SelectDistinct : QueryTag {};       // SelectDistinctExpression
        struct SelectMixedTypes : QueryTag {};     // SelectMixedTypesExpression
        struct SelectFromRecord : QueryTag {};     // SelectFromRecordExpression
        struct SelectFromTableName : QueryTag {};  // SelectFromTableNameExpression
        struct SelectWithWhere : QueryTag {};      // SelectWithWhereExpression
        struct SelectWithJoin : QueryTag {};       // SelectWithJoinExpression
        struct SelectWithGroupBy : QueryTag {};    // SelectWithGroupByExpression
        struct SelectWithHaving : QueryTag {};     // SelectWithHavingExpression
        struct SelectWithOrderBy : QueryTag {};    // SelectWithOrderByExpression
        struct SelectWithLimit : QueryTag {};      // SelectWithLimitExpression
    }  // namespace sel

    // INSERT query tags - mirrors db_insert_queries_test.cpp
    namespace ins {
        struct BasicInsert : QueryTag {};           // BasicInsertExpression
        struct InsertWithTableName : QueryTag {};   // InsertWithTableNameExpression
        struct InsertWithRecord : QueryTag {};      // InsertWithRecordExpression
        struct InsertBatch : QueryTag {};           // InsertBatchExpression
        struct InsertMultipleValues : QueryTag {};  // InsertMultipleValuesExpression
    }  // namespace ins

    // UPDATE query tags - mirrors db_update_queries_test.cpp
    namespace upd {
        struct BasicUpdate : QueryTag {};            // BasicUpdateExpression
        struct UpdateWithTableName : QueryTag {};    // UpdateWithTableNameExpression
        struct UpdateMultipleSet : QueryTag {};      // UpdateMultipleSetExpression
        struct UpdateInitializerList : QueryTag {};  // UpdateInitializerListSetExpression
        struct UpdateWithoutWhere : QueryTag {};     // UpdateWithoutWhereExpression
        struct UpdateVariousTypes : QueryTag {};     // UpdateVariousValueTypesExpression
    }  // namespace upd

    // DELETE query tags - mirrors db_delete_queries_test.cpp
    namespace del {
        struct BasicDelete : QueryTag {};          // BasicDeleteExpression
        struct DeleteWithTableName : QueryTag {};  // DeleteWithTableNameExpression
        struct DeleteWithoutWhere : QueryTag {};   // DeleteWithoutWhereExpression
        struct DeleteWhere : QueryTag {};          // DeleteWhereExpression
        struct DeleteComplexWhere : QueryTag {};   // DeleteComplexWhereExpression
        struct DeleteWithIn : QueryTag {};         // DeleteWithInExpression
        struct DeleteWithBetween : QueryTag {};    // DeleteWithBetweenExpression
        struct DeleteWithSubquery : QueryTag {};   // DeleteWithSubqueryExpression
    }  // namespace del

    // JOIN query tags - mirrors db_join_queries_test.cpp
    namespace join {
        struct InnerJoin : QueryTag {};             // InnerJoinExpression
        struct LeftJoin : QueryTag {};              // LeftJoinExpression
        struct RightJoin : QueryTag {};             // RightJoinExpression
        struct FullJoin : QueryTag {};              // FullJoinExpression
        struct CrossJoin : QueryTag {};             // CrossJoinExpression
        struct MultipleJoins : QueryTag {};         // MultipleJoinsExpression
        struct JoinComplexCondition : QueryTag {};  // JoinWithComplexConditionsExpression
        struct JoinWithWhere : QueryTag {};         // JoinWithWhereExpression
        struct JoinWithAggregates : QueryTag {};    // JoinWithAggregatesExpression
        struct JoinWithOrderBy : QueryTag {};       // JoinWithOrderByExpression
    }  // namespace join

    // AGGREGATE query tags - mirrors db_aggregate_queries_test.cpp
    namespace aggregate {
        struct Count : QueryTag {};                // COUNT
        struct Sum : QueryTag {};                  // SUM
        struct Avg : QueryTag {};                  // AVG
        struct Min : QueryTag {};                  // MIN
        struct Max : QueryTag {};                  // MAX
        struct AggregateWithAlias : QueryTag {};   // AggregateWithAliasExpressions
        struct CountDistinct : QueryTag {};        // CountDistinctExpression
        struct CountAll : QueryTag {};             // CountAllExpression
        struct AggregateGroupBy : QueryTag {};     // AggregateWithGroupByExpression
        struct AggregateHaving : QueryTag {};      // AggregateWithHavingExpression
        struct MultipleAggregates : QueryTag {};   // MultipleAggregatesExpression
        struct AggregateMixedTypes : QueryTag {};  // AggregateWithMixedTypesExpression
    }  // namespace aggregate

    // SUBQUERY query tags - mirrors db_subquery_queries_test.cpp
    namespace subq {
        struct SubqueryInWhere : QueryTag {};         // SubqueryInWhereExpression
        struct Exists : QueryTag {};                  // ExistsExpression
        struct NotExists : QueryTag {};               // NotExistsExpression
        struct BasicSubquery : QueryTag {};           // BasicSubqueryCompilationExpression
        struct SubqueryStructure : QueryTag {};       // SubqueryStructureExpression
        struct InSubqueryMultiple : QueryTag {};      // InSubqueryMultipleValuesExpression
        struct NestedSubqueries : QueryTag {};        // NestedSubqueriesExpression
        struct SubqueryWithAggregates : QueryTag {};  // SubqueryWithAggregatesExpression
        struct SubqueryWithDistinct : QueryTag {};    // SubqueryWithDistinctExpression
    }  // namespace subq

    // CONDITION query tags - mirrors db_condition_queries_test.cpp
    namespace condition {
        struct BinaryEqual : QueryTag {};         // eq
        struct BinaryNotEqual : QueryTag {};      // neq
        struct BinaryGreater : QueryTag {};       // gt
        struct BinaryGreaterEqual : QueryTag {};  // gte
        struct BinaryLess : QueryTag {};          // lt
        struct BinaryLessEqual : QueryTag {};     // lte
        struct LogicalAnd : QueryTag {};          // and
        struct LogicalOr : QueryTag {};           // or
        struct UnaryCondition : QueryTag {};      // UnaryConditionExpressions
        struct StringComparison : QueryTag {};    // StringComparisonExpressions
        struct Between : QueryTag {};             // BetweenExpressions
        struct InList : QueryTag {};              // InListExpressions
        struct ExistsCondition : QueryTag {};     // ExistsExpressions
        struct SubqueryCondition : QueryTag {};   // SubqueryConditions
        struct ComplexNested : QueryTag {};       // ComplexNestedConditions
    }  // namespace condition

    // CLAUSE query tags - mirrors db_clause_queries_test.cpp
    namespace clause {
        struct FromTable : QueryTag {};              // FROM with Table
        struct FromTableName : QueryTag {};          // FROM with table name string
        struct WhereSimple : QueryTag {};            // Simple WHERE
        struct WhereComplex : QueryTag {};           // WHERE with AND/OR
        struct WhereIn : QueryTag {};                // WHERE with IN
        struct WhereBetween : QueryTag {};           // WHERE with BETWEEN
        struct GroupBySingle : QueryTag {};          // Single column GROUP BY
        struct GroupByMultiple : QueryTag {};        // Multiple column GROUP BY
        struct GroupByWithWhere : QueryTag {};       // GROUP BY with WHERE
        struct HavingSimple : QueryTag {};           // HAVING with aggregate condition
        struct HavingMultiple : QueryTag {};         // HAVING with multiple conditions
        struct HavingWithWhere : QueryTag {};        // HAVING with WHERE and GROUP BY
        struct OrderByAsc : QueryTag {};             // ORDER BY ASC
        struct OrderByDesc : QueryTag {};            // ORDER BY DESC
        struct OrderByMultiple : QueryTag {};        // Multiple column ORDER BY
        struct LimitBasic : QueryTag {};             // Basic LIMIT
        struct LimitWithOrderBy : QueryTag {};       // LIMIT with ORDER BY
        struct LimitWithWhereOrderBy : QueryTag {};  // LIMIT with WHERE and ORDER BY
        struct ComplexAllClauses : QueryTag {};      // Complex query with all clauses
        struct ClausesWithJoins : QueryTag {};       // Clauses with JOINs
    }  // namespace clause

    // CTE (Common Table Expression) query tags
    namespace cte {
        struct BasicCte : QueryTag {};               // Basic WITH clause
        struct CteWithSelect : QueryTag {};          // CTE used in SELECT
        struct CteWithJoin : QueryTag {};            // CTE joined with table
        struct MultipleCtes : QueryTag {};           // Multiple CTEs
        struct CteWithAggregates : QueryTag {};      // CTE with aggregate functions
    }  // namespace cte

    // CASE expression query tags
    namespace case_expr {
        struct SimpleCaseWhen : QueryTag {};         // Simple CASE WHEN
        struct CaseWithElse : QueryTag {};           // CASE with ELSE
        struct CaseMultipleWhen : QueryTag {};       // CASE with multiple WHEN clauses
        struct CaseInSelect : QueryTag {};           // CASE in SELECT columns
        struct CaseWithComparison : QueryTag {};     // CASE with comparison operators
        struct CaseNested : QueryTag {};             // Nested CASE expressions
    }  // namespace case_expr

    // SET operation query tags
    namespace set_op {
        struct UnionBasic : QueryTag {};             // Basic UNION
        struct UnionAll : QueryTag {};               // UNION ALL
        struct Intersect : QueryTag {};              // INTERSECT
        struct Except : QueryTag {};                 // EXCEPT
        struct UnionWithOrderBy : QueryTag {};       // UNION with ORDER BY
        struct UnionWithLimit : QueryTag {};         // UNION with LIMIT
        struct MultipleUnions : QueryTag {};         // Chained UNIONs
        struct MixedSetOps : QueryTag {};            // Mixed set operations
    }  // namespace set_op

}  // namespace demiplane::test
