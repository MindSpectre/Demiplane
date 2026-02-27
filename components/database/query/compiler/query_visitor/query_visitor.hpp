#pragma once

#include <db_core_objects.hpp>
#include <gears_utils.hpp>

#include "query_expressions.hpp"
// NOLINTBEGIN(bugprone-use-after-move)
namespace demiplane::db {
    template <typename Derived>
    class QueryVisitor {
        constexpr Derived& derived() noexcept {
            return static_cast<Derived&>(*this);
        }
        constexpr const Derived& derived() const noexcept {
            return static_cast<const Derived&>(*this);
        }

    public:
        // Column and literals - now with perfect forwarding

        constexpr void visit(const DynamicColumn& col) {
            derived().visit_dynamic_column_impl(col.name(), col.context());
        }

        template <typename T>
        constexpr void visit(const TableColumn<T>& col) {
            derived().visit_table_column_impl(col.schema(), col.table_name(), col.alias());
        }


        template <typename T>
        constexpr void visit(Literal<T>&& lit) {
            if constexpr (std::is_same_v<T, FieldValue>) {
                derived().visit_value_impl(std::move(lit).value());
            } else {
                derived().visit_value_impl(FieldValue{lit.value()});
            }
            derived().visit_alias_impl(lit.alias());
        }

        template <typename T>
        constexpr void visit(const Literal<T>& lit) {
            if constexpr (std::is_same_v<T, FieldValue>) {
                derived().visit_value_impl(lit.value());
            } else {
                derived().visit_value_impl(FieldValue{lit.value()});
            }
            derived().visit_alias_impl(lit.alias());
        }

        constexpr void visit(const NullLiteral& null) {
            gears::unused_value(null);
            derived().visit_null_impl();
        }

        constexpr void visit(const AllColumns& all) {
            derived().visit_all_columns_impl(all.table_name());
        }

        // Expressions - perfect forwarding versions
        template <typename L, typename R, IsOperator Op>
        constexpr void visit(BinaryExpr<L, R, Op>&& expr) {
            derived().visit_binary_expr_start();
            std::move(expr).left().accept(derived());
            derived().visit_binary_op_impl(Op{});
            std::move(expr).right().accept(derived());
            derived().visit_binary_expr_end();
        }

        template <typename L, typename R, IsOperator Op>
        constexpr void visit(const BinaryExpr<L, R, Op>& expr) {
            derived().visit_binary_expr_start();
            expr.left().accept(derived());
            derived().visit_binary_op_impl(Op{});
            expr.right().accept(derived());
            derived().visit_binary_expr_end();
        }

        template <typename O, IsOperator Op>
        constexpr void visit(UnaryExpr<O, Op>&& expr) {
            derived().visit_unary_expr_start();
            if constexpr (requires { Op::is_postfix; }) {
                derived().visit(std::move(expr).operand());
                derived().visit_unary_op_impl(Op{});
            } else {
                derived().visit_unary_op_impl(Op{});
                derived().visit(std::move(expr).operand());
            }
            derived().visit_unary_expr_end();
        }

        template <typename O, IsOperator Op>
        constexpr void visit(const UnaryExpr<O, Op>& expr) {
            derived().visit_unary_expr_start();
            if constexpr (requires { Op::is_postfix; }) {
                derived().visit(expr.operand());
                derived().visit_unary_op_impl(Op{});
            } else {
                derived().visit_unary_op_impl(Op{});
                derived().visit(expr.operand());
            }
            derived().visit_unary_expr_end();
        }

        template <typename O, typename L, typename U>
        constexpr void visit(BetweenExpr<O, L, U>&& expr) {
            std::move(expr).operand().accept(derived());
            derived().visit_between_impl();
            std::move(expr).lower().accept(derived());
            derived().visit_and_impl();
            std::move(expr).upper().accept(derived());
        }

        template <typename O, typename L, typename U>
        constexpr void visit(const BetweenExpr<O, L, U>& expr) {
            expr.operand().accept(derived());
            derived().visit_between_impl();
            expr.lower().accept(derived());
            derived().visit_and_impl();
            expr.upper().accept(derived());
        }

        template <typename O, typename... Values>
        constexpr void visit(InListExpr<O, Values...>&& expr) {
            std::move(expr).operand().accept(derived());
            derived().visit_in_list_start();
            visit_tuple_elements(std::move(expr).values(), std::index_sequence_for<Values...>{});
            derived().visit_in_list_end();
        }

        template <typename O, typename... Values>
        constexpr void visit(const InListExpr<O, Values...>& expr) {
            expr.operand().accept(derived());
            derived().visit_in_list_start();
            visit_tuple_elements(expr.values(), std::index_sequence_for<Values...>{});
            derived().visit_in_list_end();
        }

        template <typename Q>
        constexpr void visit(Subquery<Q>&& sq) {
            derived().visit_subquery_start();
            std::move(sq).query().accept(derived());
            derived().visit_subquery_end();
            derived().visit_alias_impl(sq.alias());
        }

        template <typename Q>
        constexpr void visit(const Subquery<Q>& sq) {
            derived().visit_subquery_start();
            sq.query().accept(derived());
            derived().visit_subquery_end();
            derived().visit_alias_impl(sq.alias());
        }

        template <typename Q>
        constexpr void visit(ExistsExpr<Q>&& expr) {
            derived().visit_exists_start();
            std::move(expr).query().accept(derived());
            derived().visit_exists_end();
        }

        template <typename Q>
        constexpr void visit(const ExistsExpr<Q>& expr) {
            derived().visit_exists_start();
            expr.query().accept(derived());
            derived().visit_exists_end();
        }

        // Aggregate functions - perfect forwarding

        constexpr void visit(const CountExpr& expr) {
            derived().visit_count_impl(expr.distinct());
            if (expr.is_all_columns()) {
                derived().visit_all_columns_impl("");
            } else {
                expr.column().accept(derived());
            }
            derived().visit_aggregate_end(expr.alias());
        }


        constexpr void visit(const SumExpr& expr) {
            derived().visit_sum_impl();
            expr.column().accept(derived());
            derived().visit_aggregate_end(expr.alias());
        }


        constexpr void visit(const AvgExpr& expr) {
            derived().visit_avg_impl();
            expr.column().accept(derived());
            derived().visit_aggregate_end(expr.alias());
        }


        constexpr void visit(const MaxExpr& expr) {
            derived().visit_max_impl();
            expr.column().accept(derived());
            derived().visit_aggregate_end(expr.alias());
        }


        constexpr void visit(const MinExpr& expr) {
            derived().visit_min_impl();
            expr.column().accept(derived());
            derived().visit_aggregate_end(expr.alias());
        }

        // Order by - perfect forwarding

        constexpr void visit(const OrderBy& order) {
            order.column().accept(derived());
            derived().visit_order_direction_impl(order.direction());
        }

        // Query builders - perfect forwarding
        template <typename... Columns>
        constexpr void visit(SelectExpr<Columns...>&& expr) {
            derived().visit_select_start(expr.distinct());
            visit_tuple_elements(std::move(expr).columns(), std::index_sequence_for<Columns...>{});
            derived().visit_select_end();
        }

        template <typename... Columns>
        constexpr void visit(const SelectExpr<Columns...>& expr) {
            derived().visit_select_start(expr.distinct());
            visit_tuple_elements(expr.columns(), std::index_sequence_for<Columns...>{});
            derived().visit_select_end();
        }

        template <typename SelectQuery, typename IsTable>
        constexpr void visit(FromTableExpr<SelectQuery, IsTable>&& expr) {
            std::move(expr).select().accept(derived());
            derived().visit_from_start();
            derived().visit_table_impl(expr.table());
            derived().visit_alias_impl(expr.alias());
            derived().visit_from_end();
        }

        template <typename SelectQuery, typename IsTable>
        constexpr void visit(const FromTableExpr<SelectQuery, IsTable>& expr) {
            expr.select().accept(derived());
            derived().visit_from_start();
            derived().visit_table_impl(expr.table());
            derived().visit_alias_impl(expr.alias());
            derived().visit_from_end();
        }

        template <IsSelectExpr SelectQuery, IsCteExpr CteQuery>
        constexpr void visit(FromCteExpr<SelectQuery, CteQuery>&& expr) {
            derived().visit(std::move(expr).cte_query());
            std::move(expr).select().accept(derived());
            derived().visit_from_start();
            // Safe: CTE visitor only moves query_, cte_name_ remains valid
            derived().visit_table_impl(std::move(expr).cte_query().name());
            derived().visit_from_end();
        }

        template <IsSelectExpr SelectQuery, IsCteExpr CteQuery>
        constexpr void visit(const FromCteExpr<SelectQuery, CteQuery>& expr) {
            derived().visit(expr.cte_query());
            expr.select().accept(derived());
            derived().visit_from_start();
            derived().visit_table_impl(expr.cte_query().name());
            derived().visit_from_end();
        }

        template <typename Q, typename C>
        constexpr void visit(WhereExpr<Q, C>&& expr) {
            std::move(expr).query().accept(derived());
            derived().visit_where_start();
            std::move(expr).condition().accept(derived());
            derived().visit_where_end();
        }

        template <typename Q, typename C>
        constexpr void visit(const WhereExpr<Q, C>& expr) {
            expr.query().accept(derived());
            derived().visit_where_start();
            expr.condition().accept(derived());
            derived().visit_where_end();
        }

        template <typename PreGroupQuery, typename... Columns>
        constexpr void visit(GroupByColumnExpr<PreGroupQuery, Columns...>&& expr) {
            std::move(expr).query().accept(derived());
            derived().visit_group_by_start();
            visit_tuple_elements(std::move(expr).columns(), std::index_sequence_for<Columns...>{});
            derived().visit_group_by_end();
        }

        template <typename PreGroupQuery, typename... Columns>
        constexpr void visit(const GroupByColumnExpr<PreGroupQuery, Columns...>& expr) {
            expr.query().accept(derived());
            derived().visit_group_by_start();
            visit_tuple_elements(expr.columns(), std::index_sequence_for<Columns...>{});
            derived().visit_group_by_end();
        }

        template <typename PreGroupQuery, typename Criteria>
        constexpr void visit(GroupByQueryExpr<PreGroupQuery, Criteria>&& expr) {
            std::move(expr).query().accept(derived());
            derived().visit_group_by_start();
            derived().visit(std::move(expr).criteria());
            derived().visit_group_by_end();
        }

        template <typename PreGroupQuery, typename Criteria>
        constexpr void visit(const GroupByQueryExpr<PreGroupQuery, Criteria>& expr) {
            expr.query().accept(derived());
            derived().visit_group_by_start();
            derived().visit(expr.criteria());
            derived().visit_group_by_end();
        }

        template <typename Q, typename C>
        constexpr void visit(HavingExpr<Q, C>&& expr) {
            std::move(expr).query().accept(derived());
            derived().visit_having_start();
            std::move(expr).condition().accept(derived());
            derived().visit_having_end();
        }

        template <typename Q, typename C>
        constexpr void visit(const HavingExpr<Q, C>& expr) {
            expr.query().accept(derived());
            derived().visit_having_start();
            expr.condition().accept(derived());
            derived().visit_having_end();
        }

        template <typename Q, typename... O>
        constexpr void visit(OrderByExpr<Q, O...>&& expr) {
            std::move(expr).query().accept(derived());
            derived().visit_order_by_start();
            visit_tuple_elements(std::move(expr).orders(), std::index_sequence_for<O...>{});
            derived().visit_order_by_end();
        }

        template <typename Q, typename... O>
        constexpr void visit(const OrderByExpr<Q, O...>& expr) {
            expr.query().accept(derived());
            derived().visit_order_by_start();
            visit_tuple_elements(expr.orders(), std::index_sequence_for<O...>{});
            derived().visit_order_by_end();
        }

        template <typename Q>
        constexpr void visit(LimitExpr<Q>&& expr) {
            std::move(expr).query().accept(derived());
            derived().visit_limit_impl(expr.count(), expr.offset());
        }

        template <typename Q>
        constexpr void visit(const LimitExpr<Q>& expr) {
            expr.query().accept(derived());
            derived().visit_limit_impl(expr.count(), expr.offset());
        }

        template <typename Query, typename Condition, typename TableT>
        constexpr void visit(JoinExpr<Query, Condition, TableT>&& expr) {
            std::move(expr).query().accept(derived());
            derived().visit_join_start(expr.type());
            derived().visit_table_impl(std::move(expr).table());
            derived().visit_alias_impl(expr.alias());
            derived().visit_join_on();
            std::move(expr).on_condition().accept(derived());
            derived().visit_join_end();
        }

        template <typename Query, typename Condition, typename TableT>
        constexpr void visit(const JoinExpr<Query, Condition, TableT>& expr) {
            expr.query().accept(derived());
            derived().visit_join_start(expr.type());
            derived().visit_table_impl(expr.table());
            derived().visit_alias_impl(expr.alias());
            derived().visit_join_on();
            expr.on_condition().accept(derived());
            derived().visit_join_end();
        }

        // DML operations - perfect forwarding
        template <IsTable TableT>
        constexpr void visit(InsertExpr<TableT>&& expr) {
            derived().visit_insert_start();
            derived().visit_table_impl(std::move(expr).table());
            derived().visit_insert_columns(std::move(expr).columns());
            derived().visit_insert_values(std::move(expr).rows());
            derived().visit_insert_end();
        }

        template <IsTable TableT>
        constexpr void visit(const InsertExpr<TableT>& expr) {
            derived().visit_insert_start();
            derived().visit_table_impl(expr.table());
            derived().visit_insert_columns(expr.columns());
            derived().visit_insert_values(expr.rows());
            derived().visit_insert_end();
        }

        template <IsTable TableT>
        constexpr void visit(UpdateExpr<TableT>&& expr) {
            derived().visit_update_start();
            derived().visit_table_impl(std::move(expr).table());
            derived().visit_update_set(std::move(expr).assignments());
            derived().visit_update_end();
        }

        template <IsTable TableT>
        constexpr void visit(const UpdateExpr<TableT>& expr) {
            derived().visit_update_start();
            derived().visit_table_impl(expr.table());
            derived().visit_update_set(expr.assignments());
            derived().visit_update_end();
        }

        template <IsTable TableT, IsCondition ConditionT>
        constexpr void visit(UpdateWhereExpr<TableT, ConditionT>&& expr) {
            std::move(expr).update().accept(derived());
            derived().visit_where_start();
            std::move(expr).condition().accept(derived());
            derived().visit_where_end();
        }

        template <IsTable TableT, IsCondition ConditionT>
        constexpr void visit(const UpdateWhereExpr<TableT, ConditionT>& expr) {
            expr.update().accept(derived());
            derived().visit_where_start();
            expr.condition().accept(derived());
            derived().visit_where_end();
        }

        template <typename T>
        constexpr void visit(const DeleteExpr<T>& expr) {
            derived().visit_delete_start();
            derived().visit_table_impl(expr.table());
            derived().visit_delete_end();
        }

        template <typename T, typename C>
        constexpr void visit(DeleteWhereExpr<T, C>&& expr) {
            std::move(expr).del().accept(derived());
            derived().visit_where_start();
            std::move(expr).condition().accept(derived());
            derived().visit_where_end();
        }

        template <typename T, typename C>
        constexpr void visit(const DeleteWhereExpr<T, C>& expr) {
            expr.del().accept(derived());
            derived().visit_where_start();
            expr.condition().accept(derived());
            derived().visit_where_end();
        }

        // Set operations - perfect forwarding
        template <typename L, typename R>
        constexpr void visit(SetOpExpr<L, R>&& expr) {
            std::move(expr).left().accept(derived());
            derived().visit_set_op_impl(expr.op());
            std::move(expr).right().accept(derived());
        }

        template <typename L, typename R>
        constexpr void visit(const SetOpExpr<L, R>& expr) {
            expr.left().accept(derived());
            derived().visit_set_op_impl(expr.op());
            expr.right().accept(derived());
        }

        // Case expressions - perfect forwarding
        template <typename... WhenClauses>
        constexpr void visit(CaseExpr<WhenClauses...>&& expr) {
            derived().visit_case_start();
            std::apply(
                [this]<typename... WhenClauseT>(WhenClauseT&&... when_clauses) {
                    (...,
                     (derived().visit_when_start(),
                      std::forward<WhenClauseT>(when_clauses).condition.accept(derived()),
                      derived().visit_when_then(),
                      std::forward<WhenClauseT>(when_clauses).value.accept(derived()),
                      derived().visit_when_end()));
                },
                std::move(expr).when_clauses());
            derived().visit_case_end();
            derived().visit_alias_impl(expr.alias());
        }

        template <typename... WhenClauses>
        constexpr void visit(const CaseExpr<WhenClauses...>& expr) {
            derived().visit_case_start();
            std::apply(
                [this](const auto&... when_clauses) {
                    (...,
                     (derived().visit_when_start(),
                      when_clauses.condition.accept(derived()),
                      derived().visit_when_then(),
                      when_clauses.value.accept(derived()),
                      derived().visit_when_end()));
                },
                expr.when_clauses());
            derived().visit_case_end();
            derived().visit_alias_impl(expr.alias());
        }

        template <typename ElseExpr, typename... WhenClauses>
        constexpr void visit(CaseExprWithElse<ElseExpr, WhenClauses...>&& expr) {
            derived().visit_case_start();
            std::apply(
                [this]<typename... WhenClauseT>(WhenClauseT&&... when_clauses) {
                    (...,
                     (derived().visit_when_start(),
                      std::forward<WhenClauseT>(when_clauses).condition.accept(derived()),
                      derived().visit_when_then(),
                      std::forward<WhenClauseT>(when_clauses).value.accept(derived()),
                      derived().visit_when_end()));
                },
                std::move(expr).when_clauses());
            derived().visit_else_start();
            std::move(expr).else_clause().accept(derived());
            derived().visit_else_end();
            derived().visit_case_end();
            derived().visit_alias_impl(expr.alias());
        }

        template <typename ElseExpr, typename... WhenClauses>
        constexpr void visit(const CaseExprWithElse<ElseExpr, WhenClauses...>& expr) {
            derived().visit_case_start();
            std::apply(
                [this](const auto&... when_clauses) {
                    (...,
                     (derived().visit_when_start(),
                      when_clauses.condition.accept(derived()),
                      derived().visit_when_then(),
                      when_clauses.value.accept(derived()),
                      derived().visit_when_end()));
                },
                expr.when_clauses());
            derived().visit_else_start();
            expr.else_clause().accept(derived());
            derived().visit_else_end();
            derived().visit_case_end();
            derived().visit_alias_impl(expr.alias());
        }

        // CTE expressions - perfect forwarding
        template <typename Query>
        constexpr void visit(CteExpr<Query>&& expr) {
            derived().visit_cte_start(expr.recursive());
            derived().visit_cte_name_impl(expr.name());
            derived().visit_cte_as_start();
            std::move(expr).query().accept(derived());
            derived().visit_cte_as_end();
            derived().visit_cte_end();
        }

        template <typename Query>
        constexpr void visit(const CteExpr<Query>& expr) {
            derived().visit_cte_start(expr.recursive());
            derived().visit_cte_name_impl(expr.name());
            derived().visit_cte_as_start();
            expr.query().accept(derived());
            derived().visit_cte_as_end();
            derived().visit_cte_end();
        }

        // DDL operations - CREATE TABLE
        template <IsTable TableT>
        constexpr void visit(CreateTableExpr<TableT>&& expr) {
            derived().visit_create_table_start(expr.if_not_exists());
            derived().visit_table_impl(std::move(expr).table());
            derived().visit_create_table_columns(std::move(expr).table());
            derived().visit_create_table_end();
        }

        template <IsTable TableT>
        constexpr void visit(const CreateTableExpr<TableT>& expr) {
            derived().visit_create_table_start(expr.if_not_exists());
            derived().visit_table_impl(expr.table());
            derived().visit_create_table_columns(expr.table());
            derived().visit_create_table_end();
        }

        // DDL operations - DROP TABLE
        template <IsTable TableT>
        constexpr void visit(DropTableExpr<TableT>&& expr) {
            derived().visit_drop_table_start(expr.if_exists());
            derived().visit_table_impl(std::move(expr).table());
            derived().visit_drop_table_end(expr.cascade());
        }

        template <IsTable TableT>
        constexpr void visit(const DropTableExpr<TableT>& expr) {
            derived().visit_drop_table_start(expr.if_exists());
            derived().visit_table_impl(expr.table());
            derived().visit_drop_table_end(expr.cascade());
        }

    private:
        // Helper to visit tuple elements with perfect forwarding
        template <typename Tuple, std::size_t... Is>
        constexpr void visit_tuple_elements(Tuple&& t, std::index_sequence<Is...>) {
            bool first = true;
            ((visit_tuple_element(std::get<Is>(std::forward<Tuple>(t)), first)), ...);
        }

        template <typename T>
        constexpr void visit_tuple_element(T&& elem, bool& first) {
            if (!first) {
                derived().visit_column_separator();
            }
            first = false;
            derived().visit(std::forward<T>(elem));
        }
    };
}  // namespace demiplane::db

#include "detail/query_visitor.inl"
// NOLINTEND(bugprone-use-after-move)
