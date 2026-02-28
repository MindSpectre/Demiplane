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

        constexpr auto visit(const DynamicColumn& col) {
            derived().visit_dynamic_column_impl(col.name(), col.context());
            return derived().no_params();
        }

        template <typename T>
        constexpr auto visit(const TableColumn<T>& col) {
            derived().visit_table_column_impl(col.schema(), col.table_name(), col.alias());
            return derived().no_params();
        }


        template <typename T>
        constexpr auto visit(Literal<T>&& lit) {
            if constexpr (std::is_same_v<T, FieldValue>) {
                derived().visit_value_impl(std::move(lit).value());
            } else {
                derived().visit_value_impl(FieldValue{lit.value()});
            }
            derived().visit_alias_impl(lit.alias());
            return derived().capture_param(lit.value());
        }

        template <typename T>
        constexpr auto visit(const Literal<T>& lit) {
            if constexpr (std::is_same_v<T, FieldValue>) {
                derived().visit_value_impl(lit.value());
            } else {
                derived().visit_value_impl(FieldValue{lit.value()});
            }
            derived().visit_alias_impl(lit.alias());
            return derived().capture_param(lit.value());
        }

        constexpr auto visit(const NullLiteral& null) {
            gears::unused_value(null);
            derived().visit_null_impl();
            return derived().no_params();
        }

        template <typename T>
        constexpr auto visit(const ParamPlaceholder<T>&) {
            derived().visit_param_placeholder_impl();
            return derived().no_params();
        }

        constexpr auto visit(const AllColumns& all) {
            derived().visit_all_columns_impl(all.table_name());
            return derived().no_params();
        }

        // Expressions - perfect forwarding versions
        template <typename L, typename R, IsOperator Op>
        constexpr auto visit(BinaryExpr<L, R, Op>&& expr) {
            derived().visit_binary_expr_start();
            auto lp = std::move(expr).left().accept(derived());
            derived().visit_binary_op_impl(Op{});
            auto rp = std::move(expr).right().accept(derived());
            derived().visit_binary_expr_end();
            return derived().cat_params(std::move(lp), std::move(rp));
        }

        template <typename L, typename R, IsOperator Op>
        constexpr auto visit(const BinaryExpr<L, R, Op>& expr) {
            derived().visit_binary_expr_start();
            auto lp = expr.left().accept(derived());
            derived().visit_binary_op_impl(Op{});
            auto rp = expr.right().accept(derived());
            derived().visit_binary_expr_end();
            return derived().cat_params(std::move(lp), std::move(rp));
        }

        template <typename O, IsOperator Op>
        constexpr auto visit(UnaryExpr<O, Op>&& expr) {
            derived().visit_unary_expr_start();
            if constexpr (requires { Op::is_postfix; }) {
                auto op = derived().visit(std::move(expr).operand());
                derived().visit_unary_op_impl(Op{});
                derived().visit_unary_expr_end();
                return op;
            } else {
                derived().visit_unary_op_impl(Op{});
                auto op = derived().visit(std::move(expr).operand());
                derived().visit_unary_expr_end();
                return op;
            }
        }

        template <typename O, IsOperator Op>
        constexpr auto visit(const UnaryExpr<O, Op>& expr) {
            derived().visit_unary_expr_start();
            if constexpr (requires { Op::is_postfix; }) {
                auto op = derived().visit(expr.operand());
                derived().visit_unary_op_impl(Op{});
                derived().visit_unary_expr_end();
                return op;
            } else {
                derived().visit_unary_op_impl(Op{});
                auto op = derived().visit(expr.operand());
                derived().visit_unary_expr_end();
                return op;
            }
        }

        template <typename O, typename L, typename U>
        constexpr auto visit(BetweenExpr<O, L, U>&& expr) {
            auto op = std::move(expr).operand().accept(derived());
            derived().visit_between_impl();
            auto lp = std::move(expr).lower().accept(derived());
            derived().visit_and_impl();
            auto up = std::move(expr).upper().accept(derived());
            return derived().cat_params(std::move(op), std::move(lp), std::move(up));
        }

        template <typename O, typename L, typename U>
        constexpr auto visit(const BetweenExpr<O, L, U>& expr) {
            auto op = expr.operand().accept(derived());
            derived().visit_between_impl();
            auto lp = expr.lower().accept(derived());
            derived().visit_and_impl();
            auto up = expr.upper().accept(derived());
            return derived().cat_params(std::move(op), std::move(lp), std::move(up));
        }

        template <typename O, typename... Values>
        constexpr auto visit(InListExpr<O, Values...>&& expr) {
            auto op = std::move(expr).operand().accept(derived());
            derived().visit_in_list_start();
            auto vp = visit_tuple_elements(std::move(expr).values(), std::index_sequence_for<Values...>{});
            derived().visit_in_list_end();
            return derived().cat_params(std::move(op), std::move(vp));
        }

        template <typename O, typename... Values>
        constexpr auto visit(const InListExpr<O, Values...>& expr) {
            auto op = expr.operand().accept(derived());
            derived().visit_in_list_start();
            auto vp = visit_tuple_elements(expr.values(), std::index_sequence_for<Values...>{});
            derived().visit_in_list_end();
            return derived().cat_params(std::move(op), std::move(vp));
        }

        template <typename Q>
        constexpr auto visit(Subquery<Q>&& sq) {
            derived().visit_subquery_start();
            auto qp = std::move(sq).query().accept(derived());
            derived().visit_subquery_end();
            derived().visit_alias_impl(sq.alias());
            return qp;
        }

        template <typename Q>
        constexpr auto visit(const Subquery<Q>& sq) {
            derived().visit_subquery_start();
            auto qp = sq.query().accept(derived());
            derived().visit_subquery_end();
            derived().visit_alias_impl(sq.alias());
            return qp;
        }

        template <typename Q>
        constexpr auto visit(ExistsExpr<Q>&& expr) {
            derived().visit_exists_start();
            auto qp = std::move(expr).query().accept(derived());
            derived().visit_exists_end();
            return qp;
        }

        template <typename Q>
        constexpr auto visit(const ExistsExpr<Q>& expr) {
            derived().visit_exists_start();
            auto qp = expr.query().accept(derived());
            derived().visit_exists_end();
            return qp;
        }

        // Aggregate functions - perfect forwarding

        constexpr auto visit(const CountExpr& expr) {
            derived().visit_count_impl(expr.distinct());
            if (expr.is_all_columns()) {
                derived().visit_all_columns_impl("");
            } else {
                expr.column().accept(derived());
            }
            derived().visit_aggregate_end(expr.alias());
            return derived().no_params();
        }


        constexpr auto visit(const SumExpr& expr) {
            derived().visit_sum_impl();
            expr.column().accept(derived());
            derived().visit_aggregate_end(expr.alias());
            return derived().no_params();
        }


        constexpr auto visit(const AvgExpr& expr) {
            derived().visit_avg_impl();
            expr.column().accept(derived());
            derived().visit_aggregate_end(expr.alias());
            return derived().no_params();
        }


        constexpr auto visit(const MaxExpr& expr) {
            derived().visit_max_impl();
            expr.column().accept(derived());
            derived().visit_aggregate_end(expr.alias());
            return derived().no_params();
        }


        constexpr auto visit(const MinExpr& expr) {
            derived().visit_min_impl();
            expr.column().accept(derived());
            derived().visit_aggregate_end(expr.alias());
            return derived().no_params();
        }

        // Order by - perfect forwarding

        constexpr auto visit(const OrderBy& order) {
            order.column().accept(derived());
            derived().visit_order_direction_impl(order.direction());
            return derived().no_params();
        }

        // Query builders - perfect forwarding
        template <typename... Columns>
        constexpr auto visit(SelectExpr<Columns...>&& expr) {
            derived().visit_select_start(expr.distinct());
            auto cp = visit_tuple_elements(std::move(expr).columns(), std::index_sequence_for<Columns...>{});
            derived().visit_select_end();
            return cp;
        }

        template <typename... Columns>
        constexpr auto visit(const SelectExpr<Columns...>& expr) {
            derived().visit_select_start(expr.distinct());
            auto cp = visit_tuple_elements(expr.columns(), std::index_sequence_for<Columns...>{});
            derived().visit_select_end();
            return cp;
        }

        template <typename SelectQuery, typename IsTable>
        constexpr auto visit(FromTableExpr<SelectQuery, IsTable>&& expr) {
            auto sp = std::move(expr).select().accept(derived());
            derived().visit_from_start();
            derived().visit_table_impl(expr.table());
            derived().visit_alias_impl(expr.alias());
            derived().visit_from_end();
            return sp;
        }

        template <typename SelectQuery, typename IsTable>
        constexpr auto visit(const FromTableExpr<SelectQuery, IsTable>& expr) {
            auto sp = expr.select().accept(derived());
            derived().visit_from_start();
            derived().visit_table_impl(expr.table());
            derived().visit_alias_impl(expr.alias());
            derived().visit_from_end();
            return sp;
        }

        template <IsSelectExpr SelectQuery, IsCteExpr CteQuery>
        constexpr auto visit(FromCteExpr<SelectQuery, CteQuery>&& expr) {
            auto cp = derived().visit(std::move(expr).cte_query());
            auto sp = std::move(expr).select().accept(derived());
            derived().visit_from_start();
            // Safe: CTE visitor only moves query_, cte_name_ remains valid
            derived().visit_table_impl(std::move(expr).cte_query().name());
            derived().visit_from_end();
            return derived().cat_params(std::move(cp), std::move(sp));
        }

        template <IsSelectExpr SelectQuery, IsCteExpr CteQuery>
        constexpr auto visit(const FromCteExpr<SelectQuery, CteQuery>& expr) {
            auto cp = derived().visit(expr.cte_query());
            auto sp = expr.select().accept(derived());
            derived().visit_from_start();
            derived().visit_table_impl(expr.cte_query().name());
            derived().visit_from_end();
            return derived().cat_params(std::move(cp), std::move(sp));
        }

        template <typename Q, typename C>
        constexpr auto visit(WhereExpr<Q, C>&& expr) {
            auto qp = std::move(expr).query().accept(derived());
            derived().visit_where_start();
            auto cp = std::move(expr).condition().accept(derived());
            derived().visit_where_end();
            return derived().cat_params(std::move(qp), std::move(cp));
        }

        template <typename Q, typename C>
        constexpr auto visit(const WhereExpr<Q, C>& expr) {
            auto qp = expr.query().accept(derived());
            derived().visit_where_start();
            auto cp = expr.condition().accept(derived());
            derived().visit_where_end();
            return derived().cat_params(std::move(qp), std::move(cp));
        }

        template <typename PreGroupQuery, typename... Columns>
        constexpr auto visit(GroupByColumnExpr<PreGroupQuery, Columns...>&& expr) {
            auto qp = std::move(expr).query().accept(derived());
            derived().visit_group_by_start();
            auto cp = visit_tuple_elements(std::move(expr).columns(), std::index_sequence_for<Columns...>{});
            derived().visit_group_by_end();
            return derived().cat_params(std::move(qp), std::move(cp));
        }

        template <typename PreGroupQuery, typename... Columns>
        constexpr auto visit(const GroupByColumnExpr<PreGroupQuery, Columns...>& expr) {
            auto qp = expr.query().accept(derived());
            derived().visit_group_by_start();
            auto cp = visit_tuple_elements(expr.columns(), std::index_sequence_for<Columns...>{});
            derived().visit_group_by_end();
            return derived().cat_params(std::move(qp), std::move(cp));
        }

        template <typename PreGroupQuery, typename Criteria>
        constexpr auto visit(GroupByQueryExpr<PreGroupQuery, Criteria>&& expr) {
            auto qp = std::move(expr).query().accept(derived());
            derived().visit_group_by_start();
            auto cp = derived().visit(std::move(expr).criteria());
            derived().visit_group_by_end();
            return derived().cat_params(std::move(qp), std::move(cp));
        }

        template <typename PreGroupQuery, typename Criteria>
        constexpr auto visit(const GroupByQueryExpr<PreGroupQuery, Criteria>& expr) {
            auto qp = expr.query().accept(derived());
            derived().visit_group_by_start();
            auto cp = derived().visit(expr.criteria());
            derived().visit_group_by_end();
            return derived().cat_params(std::move(qp), std::move(cp));
        }

        template <typename Q, typename C>
        constexpr auto visit(HavingExpr<Q, C>&& expr) {
            auto qp = std::move(expr).query().accept(derived());
            derived().visit_having_start();
            auto cp = std::move(expr).condition().accept(derived());
            derived().visit_having_end();
            return derived().cat_params(std::move(qp), std::move(cp));
        }

        template <typename Q, typename C>
        constexpr auto visit(const HavingExpr<Q, C>& expr) {
            auto qp = expr.query().accept(derived());
            derived().visit_having_start();
            auto cp = expr.condition().accept(derived());
            derived().visit_having_end();
            return derived().cat_params(std::move(qp), std::move(cp));
        }

        template <typename Q, typename... O>
        constexpr auto visit(OrderByExpr<Q, O...>&& expr) {
            auto qp = std::move(expr).query().accept(derived());
            derived().visit_order_by_start();
            auto op = visit_tuple_elements(std::move(expr).orders(), std::index_sequence_for<O...>{});
            derived().visit_order_by_end();
            return derived().cat_params(std::move(qp), std::move(op));
        }

        template <typename Q, typename... O>
        constexpr auto visit(const OrderByExpr<Q, O...>& expr) {
            auto qp = expr.query().accept(derived());
            derived().visit_order_by_start();
            auto op = visit_tuple_elements(expr.orders(), std::index_sequence_for<O...>{});
            derived().visit_order_by_end();
            return derived().cat_params(std::move(qp), std::move(op));
        }

        template <typename Q>
        constexpr auto visit(LimitExpr<Q>&& expr) {
            auto qp = std::move(expr).query().accept(derived());
            derived().visit_limit_impl(expr.count(), expr.offset());
            return qp;
        }

        template <typename Q>
        constexpr auto visit(const LimitExpr<Q>& expr) {
            auto qp = expr.query().accept(derived());
            derived().visit_limit_impl(expr.count(), expr.offset());
            return qp;
        }

        template <typename Query, typename Condition, typename TableT>
        constexpr auto visit(JoinExpr<Query, Condition, TableT>&& expr) {
            auto qp = std::move(expr).query().accept(derived());
            derived().visit_join_start(expr.type());
            derived().visit_table_impl(std::move(expr).table());
            derived().visit_alias_impl(expr.alias());
            derived().visit_join_on();
            auto cp = std::move(expr).on_condition().accept(derived());
            derived().visit_join_end();
            return derived().cat_params(std::move(qp), std::move(cp));
        }

        template <typename Query, typename Condition, typename TableT>
        constexpr auto visit(const JoinExpr<Query, Condition, TableT>& expr) {
            auto qp = expr.query().accept(derived());
            derived().visit_join_start(expr.type());
            derived().visit_table_impl(expr.table());
            derived().visit_alias_impl(expr.alias());
            derived().visit_join_on();
            auto cp = expr.on_condition().accept(derived());
            derived().visit_join_end();
            return derived().cat_params(std::move(qp), std::move(cp));
        }

        // DML operations - perfect forwarding
        template <IsTable TableT>
        constexpr auto visit(InsertExpr<TableT>&& expr) {
            derived().visit_insert_start();
            derived().visit_table_impl(std::move(expr).table());
            derived().visit_insert_columns(std::move(expr).columns());
            derived().visit_insert_values(std::move(expr).rows());
            derived().visit_insert_end();
            return derived().no_params();
        }

        template <IsTable TableT>
        constexpr auto visit(const InsertExpr<TableT>& expr) {
            derived().visit_insert_start();
            derived().visit_table_impl(expr.table());
            derived().visit_insert_columns(expr.columns());
            derived().visit_insert_values(expr.rows());
            derived().visit_insert_end();
            return derived().no_params();
        }

        template <IsTable TableT>
        constexpr auto visit(UpdateExpr<TableT>&& expr) {
            derived().visit_update_start();
            derived().visit_table_impl(std::move(expr).table());
            derived().visit_update_set(std::move(expr).assignments());
            derived().visit_update_end();
            return derived().no_params();
        }

        template <IsTable TableT>
        constexpr auto visit(const UpdateExpr<TableT>& expr) {
            derived().visit_update_start();
            derived().visit_table_impl(expr.table());
            derived().visit_update_set(expr.assignments());
            derived().visit_update_end();
            return derived().no_params();
        }

        template <IsTable TableT, IsCondition ConditionT>
        constexpr auto visit(UpdateWhereExpr<TableT, ConditionT>&& expr) {
            auto up = std::move(expr).update().accept(derived());
            derived().visit_where_start();
            auto cp = std::move(expr).condition().accept(derived());
            derived().visit_where_end();
            return derived().cat_params(std::move(up), std::move(cp));
        }

        template <IsTable TableT, IsCondition ConditionT>
        constexpr auto visit(const UpdateWhereExpr<TableT, ConditionT>& expr) {
            auto up = expr.update().accept(derived());
            derived().visit_where_start();
            auto cp = expr.condition().accept(derived());
            derived().visit_where_end();
            return derived().cat_params(std::move(up), std::move(cp));
        }

        template <typename T>
        constexpr auto visit(const DeleteExpr<T>& expr) {
            derived().visit_delete_start();
            derived().visit_table_impl(expr.table());
            derived().visit_delete_end();
            return derived().no_params();
        }

        template <typename T, typename C>
        constexpr auto visit(DeleteWhereExpr<T, C>&& expr) {
            auto dp = std::move(expr).del().accept(derived());
            derived().visit_where_start();
            auto cp = std::move(expr).condition().accept(derived());
            derived().visit_where_end();
            return derived().cat_params(std::move(dp), std::move(cp));
        }

        template <typename T, typename C>
        constexpr auto visit(const DeleteWhereExpr<T, C>& expr) {
            auto dp = expr.del().accept(derived());
            derived().visit_where_start();
            auto cp = expr.condition().accept(derived());
            derived().visit_where_end();
            return derived().cat_params(std::move(dp), std::move(cp));
        }

        // Set operations - perfect forwarding
        template <typename L, typename R>
        constexpr auto visit(SetOpExpr<L, R>&& expr) {
            auto lp = std::move(expr).left().accept(derived());
            derived().visit_set_op_impl(expr.op());
            auto rp = std::move(expr).right().accept(derived());
            return derived().cat_params(std::move(lp), std::move(rp));
        }

        template <typename L, typename R>
        constexpr auto visit(const SetOpExpr<L, R>& expr) {
            auto lp = expr.left().accept(derived());
            derived().visit_set_op_impl(expr.op());
            auto rp = expr.right().accept(derived());
            return derived().cat_params(std::move(lp), std::move(rp));
        }

        // Case expressions - perfect forwarding
        template <typename... WhenClauses>
        constexpr auto visit(CaseExpr<WhenClauses...>&& expr) {
            derived().visit_case_start();
            auto wp = visit_when_clauses(std::move(expr).when_clauses(), std::index_sequence_for<WhenClauses...>{});
            derived().visit_case_end();
            derived().visit_alias_impl(expr.alias());
            return wp;
        }

        template <typename... WhenClauses>
        constexpr auto visit(const CaseExpr<WhenClauses...>& expr) {
            derived().visit_case_start();
            auto wp = visit_when_clauses(expr.when_clauses(), std::index_sequence_for<WhenClauses...>{});
            derived().visit_case_end();
            derived().visit_alias_impl(expr.alias());
            return wp;
        }

        template <typename ElseExpr, typename... WhenClauses>
        constexpr auto visit(CaseExprWithElse<ElseExpr, WhenClauses...>&& expr) {
            derived().visit_case_start();
            auto wp = visit_when_clauses(std::move(expr).when_clauses(), std::index_sequence_for<WhenClauses...>{});
            derived().visit_else_start();
            auto ep = std::move(expr).else_clause().accept(derived());
            derived().visit_else_end();
            derived().visit_case_end();
            derived().visit_alias_impl(expr.alias());
            return derived().cat_params(std::move(wp), std::move(ep));
        }

        template <typename ElseExpr, typename... WhenClauses>
        constexpr auto visit(const CaseExprWithElse<ElseExpr, WhenClauses...>& expr) {
            derived().visit_case_start();
            auto wp = visit_when_clauses(expr.when_clauses(), std::index_sequence_for<WhenClauses...>{});
            derived().visit_else_start();
            auto ep = expr.else_clause().accept(derived());
            derived().visit_else_end();
            derived().visit_case_end();
            derived().visit_alias_impl(expr.alias());
            return derived().cat_params(std::move(wp), std::move(ep));
        }

        // CTE expressions - perfect forwarding
        template <typename Query>
        constexpr auto visit(CteExpr<Query>&& expr) {
            derived().visit_cte_start(expr.recursive());
            derived().visit_cte_name_impl(expr.name());
            derived().visit_cte_as_start();
            auto qp = std::move(expr).query().accept(derived());
            derived().visit_cte_as_end();
            derived().visit_cte_end();
            return qp;
        }

        template <typename Query>
        constexpr auto visit(const CteExpr<Query>& expr) {
            derived().visit_cte_start(expr.recursive());
            derived().visit_cte_name_impl(expr.name());
            derived().visit_cte_as_start();
            auto qp = expr.query().accept(derived());
            derived().visit_cte_as_end();
            derived().visit_cte_end();
            return qp;
        }

        // DDL operations - CREATE TABLE
        template <IsTable TableT>
        constexpr auto visit(CreateTableExpr<TableT>&& expr) {
            derived().visit_create_table_start(expr.if_not_exists());
            derived().visit_table_impl(std::move(expr).table());
            derived().visit_create_table_columns(std::move(expr).table());
            derived().visit_create_table_end();
            return derived().no_params();
        }

        template <IsTable TableT>
        constexpr auto visit(const CreateTableExpr<TableT>& expr) {
            derived().visit_create_table_start(expr.if_not_exists());
            derived().visit_table_impl(expr.table());
            derived().visit_create_table_columns(expr.table());
            derived().visit_create_table_end();
            return derived().no_params();
        }

        // DDL operations - DROP TABLE
        template <IsTable TableT>
        constexpr auto visit(DropTableExpr<TableT>&& expr) {
            derived().visit_drop_table_start(expr.if_exists());
            derived().visit_table_impl(std::move(expr).table());
            derived().visit_drop_table_end(expr.cascade());
            return derived().no_params();
        }

        template <IsTable TableT>
        constexpr auto visit(const DropTableExpr<TableT>& expr) {
            derived().visit_drop_table_start(expr.if_exists());
            derived().visit_table_impl(expr.table());
            derived().visit_drop_table_end(expr.cascade());
            return derived().no_params();
        }

    private:
        // Helper to visit tuple elements with recursive approach

        // Base case: empty index sequence
        template <typename Tuple>
        constexpr auto visit_tuple_elements(Tuple&&, std::index_sequence<>) {
            return derived().no_params();
        }

        // Recursive case: visit first, recurse rest
        template <typename Tuple, std::size_t First, std::size_t... Rest>
        constexpr auto visit_tuple_elements(Tuple&& t, std::index_sequence<First, Rest...>) {
            auto head = visit_tuple_element(std::get<First>(std::forward<Tuple>(t)), First == 0);
            auto tail = visit_tuple_elements(std::forward<Tuple>(t), std::index_sequence<Rest...>{});
            return derived().cat_params(std::move(head), std::move(tail));
        }

        template <typename T>
        constexpr auto visit_tuple_element(T&& elem, bool is_first) {
            if (!is_first) {
                derived().visit_column_separator();
            }
            return derived().visit(std::forward<T>(elem));
        }

        // Recursive when-clause visitor for CASE expressions

        // Base case: empty index sequence
        template <typename Tuple>
        constexpr auto visit_when_clauses(Tuple&&, std::index_sequence<>) {
            return derived().no_params();
        }

        // Recursive case: visit first when-clause, recurse rest
        template <typename Tuple, std::size_t First, std::size_t... Rest>
        constexpr auto visit_when_clauses(Tuple&& t, std::index_sequence<First, Rest...>) {
            auto head = visit_single_when(std::get<First>(std::forward<Tuple>(t)));
            auto tail = visit_when_clauses(std::forward<Tuple>(t), std::index_sequence<Rest...>{});
            return derived().cat_params(std::move(head), std::move(tail));
        }

        template <typename WhenClause>
        constexpr auto visit_single_when(WhenClause&& when) {
            derived().visit_when_start();
            auto cp = std::forward<WhenClause>(when).condition.accept(derived());
            derived().visit_when_then();
            auto vp = std::forward<WhenClause>(when).value.accept(derived());
            derived().visit_when_end();
            return derived().cat_params(std::move(cp), std::move(vp));
        }
    };
}  // namespace demiplane::db

#include "detail/query_visitor.inl"
// NOLINTEND(bugprone-use-after-move)
