#pragma once

#include "db_core_fwd.hpp"
#include "gears_utils.hpp"
#include "query_expressions.hpp"

namespace demiplane::db {
    class QueryVisitor {
    public:
        virtual ~QueryVisitor() = default;

        // Column and literals
        template <typename T>
        void visit(const Column<T>& col) {
            visit_column_impl(col.schema(), col.table(), col.alias());
        }

        void visit(const Column<void>& col) {
            visit_column_impl(col.schema(), col.table(), col.alias());
        }

        template <typename T>
        void visit(const Literal<T>& lit) {
            visit_literal_impl(FieldValue(lit.value));
        }

        void visit(const NullLiteral& null) {
            gears::unused_value(null);
            visit_null_impl();
        }

        void visit(const AllColumns& all) {
            visit_all_columns_impl(all.table());
        }

        void visit(const Parameter& param) {
            visit_parameter_impl(param.index);
        }

        // Expressions
        template <typename L, typename R, IsOperator Op>
        void visit(const BinaryExpr<L, R, Op>& expr) {
            visit_binary_expr_start();
            expr.left().accept(*this);
            visit_binary_op_impl(Op{});
            expr.right().accept(*this);
            visit_binary_expr_end();
        }

        template <typename O, IsOperator Op>
        void visit(const UnaryExpr<O, Op>& expr) {
            visit_unary_expr_start();
            visit_unary_op_impl(Op{});
            expr.operand().accept(*this);
            visit_unary_expr_end();
        }

        template <typename O, typename L, typename U>
        void visit(const BetweenExpr<O, L, U>& expr) {
            expr.operand().accept(*this);
            visit_between_impl();
            expr.lower().accept(*this);
            visit_and_impl();
            expr.upper().accept(*this);
        }

        template <typename O, typename... Values>
        void visit(const InListExpr<O, Values...>& expr) {
            expr.operand().accept(*this);
            visit_in_list_start();
            visit_tuple_elements(expr.values(), std::index_sequence_for<Values...>{});
            visit_in_list_end();
        }

        template <typename Q>
        void visit(const Subquery<Q>& sq) {
            visit_subquery_start();
            sq.query().accept(*this);
            visit_subquery_end();
            if (sq.alias()) {
                visit_alias_impl(sq.alias());
            }
        }

        template <typename Q>
        void visit(const ExistsExpr<Q>& expr) {
            visit_exists_start();
            expr.query().accept(*this);
            visit_exists_end();
        }

        // Aggregate functions
        template <typename T>
        void visit(const CountExpr<T>& expr) {
            visit_count_impl(expr.distinct());
            if (expr.column().schema()) {
                expr.column().accept(*this);
            }
            visit_aggregate_end(expr.alias());
        }

        template <typename T>
        void visit(const SumExpr<T>& expr) {
            visit_sum_impl();
            expr.column().accept(*this);
            visit_aggregate_end(expr.alias());
        }

        template <typename T>
        void visit(const AvgExpr<T>& expr) {
            visit_avg_impl();
            expr.column().accept(*this);
            visit_aggregate_end(expr.alias());
        }

        template <typename T>
        void visit(const MaxExpr<T>& expr) {
            visit_max_impl();
            expr.column().accept(*this);
            visit_aggregate_end(expr.alias());
        }

        template <typename T>
        void visit(const MinExpr<T>& expr) {
            visit_min_impl();
            expr.column().accept(*this);
            visit_aggregate_end(expr.alias());
        }

        // Order by
        template <typename T>
        void visit(const OrderBy<T>& order) {
            order.column().accept(*this);
            visit_order_direction_impl(order.direction());
        }

        // Query builders
        template <typename... Columns>
        void visit(const SelectExpr<Columns...>& expr) {
            visit_select_start(expr.distinct());
            visit_tuple_elements(expr.columns(), std::index_sequence_for<Columns...>{});
            visit_select_end();
        }

        template <typename S>
        void visit(const FromExpr<S>& expr) {
            expr.select().accept(*this);
            visit_from_start();
            visit_table_impl(expr.table());
            visit_alias_impl(expr.alias());
            visit_from_end();
        }

        template <typename Q, typename C>
        void visit(const WhereExpr<Q, C>& expr) {
            expr.query().accept(*this);
            visit_where_start();
            expr.condition().accept(*this);
            visit_where_end();
        }

        template <typename Q, typename... C>
        void visit(const GroupByExpr<Q, C...>& expr) {
            expr.query().accept(*this);
            visit_group_by_start();
            visit_tuple_elements(expr.columns(), std::index_sequence_for<C...>{});
            visit_group_by_end();
        }

        template <typename Q, typename C>
        void visit(const HavingExpr<Q, C>& expr) {
            expr.query().accept(*this);
            visit_having_start();
            expr.condition().accept(*this);
            visit_having_end();
        }

        template <typename Q, typename... O>
        void visit(const OrderByExpr<Q, O...>& expr) {
            expr.query().accept(*this);
            visit_order_by_start();
            visit_tuple_elements(expr.orders(), std::index_sequence_for<O...>{});
            visit_order_by_end();
        }

        template <typename Q>
        void visit(const LimitExpr<Q>& expr) {
            expr.query().accept(*this);
            visit_limit_impl(expr.count(), expr.offset());
        }

        template <typename Q, typename J, typename C>
        void visit(const JoinExpr<Q, J, C>& expr) {
            expr.query().accept(*this);
            visit_join_start(expr.type());
            visit_table_impl(expr.joined_table());
            visit_alias_impl(expr.joined_alias());
            visit_join_on();
            expr.on_condition().accept(*this);
            visit_join_end();
        }

        // DML operations
        void visit(const InsertExpr& expr) {
            visit_insert_start();
            visit_table_impl(expr.table());
            visit_insert_columns(expr.columns());
            visit_insert_values(expr.rows());
            visit_insert_end();
        }

        void visit(const UpdateExpr& expr) {
            visit_update_start();
            visit_table_impl(expr.table());
            visit_update_set(expr.assignments());
            visit_update_end();
        }

        template <typename C>
        void visit(const UpdateWhereExpr<C>& expr) {
            expr.update().accept(*this);
            visit_where_start();
            expr.condition().accept(*this);
            visit_where_end();
        }

        void visit(const DeleteExpr& expr) {
            visit_delete_start();
            visit_table_impl(expr.table());
            visit_delete_end();
        }

        template <typename C>
        void visit(const DeleteWhereExpr<C>& expr) {
            expr.del().accept(*this);
            visit_where_start();
            expr.condition().accept(*this);
            visit_where_end();
        }

        // Set operations
        template <typename L, typename R>
        void visit(const SetOpExpr<L, R>& expr) {
            expr.left().accept(*this);
            visit_set_op_impl(expr.op());
            expr.right().accept(*this);
        }

        template <typename... WhenClauses>
        void visit(const CaseExpr<WhenClauses...>& expr) {
            visit_case_start();
            visit_when_clauses(expr.when_clauses(), std::index_sequence_for<WhenClauses...>{});
            visit_case_end();
        }

        template <typename ElseExpr, typename... WhenClauses>
        void visit(const CaseExprWithElse<ElseExpr, WhenClauses...>& expr) {
            visit_case_start();
            visit_when_clauses(expr.when_clauses(), std::index_sequence_for<WhenClauses...>{});
            visit_else_start();
            expr.else_clause().accept(*this);
            visit_else_end();
            visit_case_end();
        }

        template <typename Query>
        void visit(const CteExpr<Query>& expr) {
            visit_cte_start(expr.recursive());
            visit_cte_name_impl(expr.name());
            visit_cte_as_start();
            expr.query().accept(*this);
            visit_cte_as_end();
            visit_cte_end();
        }

    protected:
        // Subclasses implement these
        virtual void visit_column_impl(const FieldSchema* schema,
                                       const std::shared_ptr<std::string>& table,
                                       const std::optional<std::string>& alias) = 0;
        virtual void visit_literal_impl(const FieldValue& value) = 0;
        virtual void visit_null_impl() = 0;
        virtual void visit_all_columns_impl(const std::shared_ptr<std::string>& table) = 0;
        virtual void visit_parameter_impl(std::size_t index) = 0;
        virtual void visit_table_impl(const TableSchemaPtr& table) = 0;
        virtual void visit_table_spec_impl(const std::shared_ptr<std::string>& table) = 0;
        virtual void visit_alias_impl(const std::optional<std::string>& alias) = 0;

        // Expression helpers
        virtual void visit_binary_expr_start() {}
        virtual void visit_binary_expr_end() {}
        virtual void visit_unary_expr_start() {}
        virtual void visit_unary_expr_end() {}
        virtual void visit_subquery_start() = 0;
        virtual void visit_subquery_end() = 0;
        virtual void visit_exists_start() = 0;
        virtual void visit_exists_end() = 0;

        // Binary operators
        virtual void visit_binary_op_impl(OpEqual) = 0;
        virtual void visit_binary_op_impl(OpNotEqual) = 0;
        virtual void visit_binary_op_impl(OpLess) = 0;
        virtual void visit_binary_op_impl(OpLessEqual) = 0;
        virtual void visit_binary_op_impl(OpGreater) = 0;
        virtual void visit_binary_op_impl(OpGreaterEqual) = 0;
        virtual void visit_binary_op_impl(OpAnd) = 0;
        virtual void visit_binary_op_impl(OpOr) = 0;
        virtual void visit_binary_op_impl(OpLike) = 0;
        virtual void visit_binary_op_impl(OpIn) = 0;
        virtual void visit_binary_op_impl(OpNotLike) = 0;

        // Unary operators
        virtual void visit_unary_op_impl(OpNot) = 0;
        virtual void visit_unary_op_impl(OpIsNull) = 0;
        virtual void visit_unary_op_impl(OpIsNotNull) = 0;

        // Special operators
        virtual void visit_between_impl() = 0;
        virtual void visit_and_impl() = 0;
        virtual void visit_in_list_start() = 0;
        virtual void visit_in_list_end() = 0;
        virtual void visit_in_list_separator() = 0;

        // Aggregate functions
        virtual void visit_count_impl(bool distinct) = 0;
        virtual void visit_sum_impl() = 0;
        virtual void visit_avg_impl() = 0;
        virtual void visit_max_impl() = 0;
        virtual void visit_min_impl() = 0;
        virtual void visit_aggregate_end(const std::optional<std::string>& alias) = 0;

        // Query parts
        virtual void visit_select_start(bool distinct) = 0;
        virtual void visit_select_end() = 0;
        virtual void visit_from_start() = 0;
        virtual void visit_from_end() = 0;
        virtual void visit_where_start() = 0;
        virtual void visit_where_end() = 0;
        virtual void visit_group_by_start() = 0;
        virtual void visit_group_by_end() = 0;
        virtual void visit_having_start() = 0;
        virtual void visit_having_end() = 0;
        virtual void visit_order_by_start() = 0;
        virtual void visit_order_by_end() = 0;
        virtual void visit_order_direction_impl(OrderDirection dir) = 0;
        virtual void visit_limit_impl(std::size_t limit, std::size_t offset) = 0;

        // Joins
        virtual void visit_join_start(JoinType type) = 0;
        virtual void visit_join_on() = 0;
        virtual void visit_join_end() = 0;

        // DML
        virtual void visit_insert_start() = 0;
        virtual void visit_insert_columns(const std::vector<std::string>& columns) = 0;
        virtual void visit_insert_values(const std::vector<std::vector<FieldValue>>& rows) = 0;
        virtual void visit_insert_end() = 0;

        virtual void visit_update_start() = 0;
        virtual void visit_update_set(const std::vector<std::pair<std::string, FieldValue>>& assignments) = 0;
        virtual void visit_update_end() = 0;

        virtual void visit_delete_start() = 0;
        virtual void visit_delete_end() = 0;

        // Set operations
        virtual void visit_set_op_impl(SetOperation op) = 0;

        virtual void visit_case_start() = 0;
        virtual void visit_case_end() = 0;
        virtual void visit_when_start() = 0;
        virtual void visit_when_then() = 0;
        virtual void visit_when_end() = 0;
        virtual void visit_else_start() = 0;
        virtual void visit_else_end() = 0;

        // CTE (Common Table Expression)
        virtual void visit_cte_start(bool recursive) = 0;
        virtual void visit_cte_name_impl(std::string_view name) = 0;
        virtual void visit_cte_as_start() = 0;
        virtual void visit_cte_as_end() = 0;
        virtual void visit_cte_end() = 0;

        // Column separator
        virtual void visit_column_separator() = 0;

    private:
        // Helper to visit tuple elements
        template <typename Tuple, std::size_t... Is>
        void visit_tuple_elements(const Tuple& t, std::index_sequence<Is...>) {
            bool first = true;
            ((visit_tuple_element(std::get<Is>(t), first)), ...);
        }

        template <typename T>
        void visit_tuple_element(const T& elem, bool& first) {
            if (!first) {
                visit_column_separator();
            }
            first = false;
            visit(elem);
        }

        template <typename Tuple, std::size_t... Is>
        void visit_when_clauses(const Tuple& t, std::index_sequence<Is...>) {
            ((visit_when_clause(std::get<Is>(t))), ...);
        }

        template <typename ConditionExpr, typename ValueExpr>
        void visit_when_clause(const WhenClause<ConditionExpr, ValueExpr>& when) {
            visit_when_start();
            when.condition.accept(*this);
            visit_when_then();
            when.value.accept(*this);
            visit_when_end();
        }
    };
}

#include "../source/query_visitor.inl"
