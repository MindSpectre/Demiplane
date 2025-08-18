#pragma once

#include "db_core_fwd.hpp"
#include "gears_utils.hpp"
#include "query_expressions.hpp"

namespace demiplane::db {
    class QueryVisitor {
    public:
        virtual ~QueryVisitor() = default;

        // Column and literals - now with perfect forwarding

        void visit(const DynamicColumn& col) {
            visit_dynamic_column_impl(col.name(), col.context());
        }

        template <typename T>
        void visit(const TableColumn<T>& col) {
            visit_table_column_impl(col.schema(), col.table(), col.alias());
        }


        template <typename T>
        void visit(Literal<T>&& lit) {
            visit_value_impl(FieldValue{std::move(lit).value()});
            visit_alias_impl(std::move(lit).alias());
        }

        template <typename T>
        void visit(const Literal<T>& lit) {
            visit_value_impl(FieldValue{lit.value()});
            visit_alias_impl(lit.alias());
        }

        void visit(const NullLiteral& null) {
            gears::unused_value(null);
            visit_null_impl();
        }

        void visit(const AllColumns& all) {
            visit_all_columns_impl(all.table());
        }

        // Expressions - perfect forwarding versions
        template <typename L, typename R, IsOperator Op>
        void visit(BinaryExpr<L, R, Op>&& expr) {
            visit_binary_expr_start();
            std::move(expr).left().accept(*this);
            visit_binary_op_impl(Op{});
            std::move(expr).right().accept(*this);
            visit_binary_expr_end();
        }

        template <typename L, typename R, IsOperator Op>
        void visit(const BinaryExpr<L, R, Op>& expr) {
            visit_binary_expr_start();
            expr.left().accept(*this);
            visit_binary_op_impl(Op{});
            expr.right().accept(*this);
            visit_binary_expr_end();
        }

        template <typename O, IsOperator Op>
        void visit(UnaryExpr<O, Op>&& expr) {
            visit_unary_expr_start();
            visit_unary_op_impl(Op{});
            visit(std::move(expr).operand());
            visit_unary_expr_end();
        }

        template <typename O, IsOperator Op>
        void visit(const UnaryExpr<O, Op>& expr) {
            visit_unary_expr_start();
            visit_unary_op_impl(Op{});
            visit(expr.operand());
            visit_unary_expr_end();
        }

        template <typename O, typename L, typename U>
        void visit(BetweenExpr<O, L, U>&& expr) {
            std::move(expr).operand().accept(*this);
            visit_between_impl();
            std::move(expr).lower().accept(*this);
            visit_and_impl();
            std::move(expr).upper().accept(*this);
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
        void visit(InListExpr<O, Values...>&& expr) {
            std::move(expr).operand().accept(*this);
            visit_in_list_start();
            visit_tuple_elements(std::move(expr).values(), std::index_sequence_for<Values...>{});
            visit_in_list_end();
        }

        template <typename O, typename... Values>
        void visit(const InListExpr<O, Values...>& expr) {
            expr.operand().accept(*this);
            visit_in_list_start();
            visit_tuple_elements(expr.values(), std::index_sequence_for<Values...>{});
            visit_in_list_end();
        }

        template <typename Q>
        void visit(Subquery<Q>&& sq) {
            visit_subquery_start();
            std::move(sq).query().accept(*this);
            visit_subquery_end();
            visit_alias_impl(sq.alias());
        }

        template <typename Q>
        void visit(const Subquery<Q>& sq) {
            visit_subquery_start();
            sq.query().accept(*this);
            visit_subquery_end();
            visit_alias_impl(sq.alias());
        }

        template <typename Q>
        void visit(ExistsExpr<Q>&& expr) {
            visit_exists_start();
            std::move(expr).query().accept(*this);
            visit_exists_end();
        }

        template <typename Q>
        void visit(const ExistsExpr<Q>& expr) {
            visit_exists_start();
            expr.query().accept(*this);
            visit_exists_end();
        }

        // Aggregate functions - perfect forwarding

        void visit(const CountExpr& expr) {
            visit_count_impl(expr.distinct());
            if (expr.is_all_columns()) {
                visit_all_columns_impl(nullptr);
            }
            else {
                expr.column().accept(*this);
            }
            visit_aggregate_end(expr.alias());
        }


        void visit(const SumExpr& expr) {
            visit_sum_impl();
            expr.column().accept(*this);
            visit_aggregate_end(expr.alias());
        }


        void visit(const AvgExpr& expr) {
            visit_avg_impl();
            expr.column().accept(*this);
            visit_aggregate_end(expr.alias());
        }


        void visit(const MaxExpr& expr) {
            visit_max_impl();
            expr.column().accept(*this);
            visit_aggregate_end(expr.alias());
        }


        void visit(const MinExpr& expr) {
            visit_min_impl();
            expr.column().accept(*this);
            visit_aggregate_end(expr.alias());
        }

        // Order by - perfect forwarding

        void visit(const OrderBy& order) {
            order.column().accept(*this);
            visit_order_direction_impl(order.direction());
        }

        // Query builders - perfect forwarding
        template <typename... Columns>
        void visit(SelectExpr<Columns...>&& expr) {
            visit_select_start(expr.distinct());
            visit_tuple_elements(std::move(expr).columns(), std::index_sequence_for<Columns...>{});
            visit_select_end();
        }

        template <typename... Columns>
        void visit(const SelectExpr<Columns...>& expr) {
            visit_select_start(expr.distinct());
            visit_tuple_elements(expr.columns(), std::index_sequence_for<Columns...>{});
            visit_select_end();
        }

        template <typename SelectQuery>
        void visit(FromTableExpr<SelectQuery>&& expr) {
            std::move(expr).select().accept(*this);
            visit_from_start();
            visit_table_impl(expr.table());
            visit_alias_impl(expr.alias());
            visit_from_end();
        }

        template <typename SelectQuery>
        void visit(const FromTableExpr<SelectQuery>& expr) {
            expr.select().accept(*this);
            visit_from_start();
            visit_table_impl(expr.table());
            visit_alias_impl(expr.alias());
            visit_from_end();
        }

        template <IsSelectExpr SelectQuery, IsCteExpr CteQuery>
        void visit(FromCteExpr<SelectQuery, CteQuery>&& expr) {
            visit(std::move(expr).cte_query());
            std::move(expr).select().accept(*this);
            visit_from_start();
            visit_table_impl(std::move(expr).cte_query().name());
            visit_from_end();
        }

        template <IsSelectExpr SelectQuery, IsCteExpr CteQuery>
        void visit(const FromCteExpr<SelectQuery, CteQuery>& expr) {
            visit(expr.cte_query());
            expr.select().accept(*this);
            visit_from_start();
            visit_table_impl(std::move(expr).cte_query().name());
            visit_from_end();
        }

        template <typename Q, typename C>
        void visit(WhereExpr<Q, C>&& expr) {
            std::move(expr).query().accept(*this);
            visit_where_start();
            std::move(expr).condition().accept(*this);
            visit_where_end();
        }

        template <typename Q, typename C>
        void visit(const WhereExpr<Q, C>& expr) {
            expr.query().accept(*this);
            visit_where_start();
            expr.condition().accept(*this);
            visit_where_end();
        }

        template <typename PreGroupQuery, typename... Columns>
        void visit(GroupByColumnExpr<PreGroupQuery, Columns...>&& expr) {
            std::move(expr).query().accept(*this);
            visit_group_by_start();
            visit_tuple_elements(std::move(expr).columns(), std::index_sequence_for<Columns...>{});
            visit_group_by_end();
        }

        template <typename PreGroupQuery, typename... Columns>
        void visit(const GroupByColumnExpr<PreGroupQuery, Columns...>& expr) {
            expr.query().accept(*this);
            visit_group_by_start();
            visit_tuple_elements(expr.columns(), std::index_sequence_for<Columns...>{});
            visit_group_by_end();
        }

        template <typename PreGroupQuery, typename Criteria>
        void visit(GroupByQueryExpr<PreGroupQuery, Criteria>&& expr) {
            std::move(expr).query().accept(*this);
            visit_group_by_start();
            visit(std::move(expr).criteria());
            visit_group_by_end();
        }

        template <typename PreGroupQuery, typename Criteria>
        void visit(const GroupByQueryExpr<PreGroupQuery, Criteria>& expr) {
            expr.query().accept(*this);
            visit_group_by_start();
            visit(expr.criteria());
            visit_group_by_end();
        }

        template <typename Q, typename C>
        void visit(HavingExpr<Q, C>&& expr) {
            std::move(expr).query().accept(*this);
            visit_having_start();
            std::move(expr).condition().accept(*this);
            visit_having_end();
        }

        template <typename Q, typename C>
        void visit(const HavingExpr<Q, C>& expr) {
            expr.query().accept(*this);
            visit_having_start();
            expr.condition().accept(*this);
            visit_having_end();
        }

        template <typename Q, typename... O>
        void visit(OrderByExpr<Q, O...>&& expr) {
            std::move(expr).query().accept(*this);
            visit_order_by_start();
            visit_tuple_elements(std::move(expr).orders(), std::index_sequence_for<O...>{});
            visit_order_by_end();
        }

        template <typename Q, typename... O>
        void visit(const OrderByExpr<Q, O...>& expr) {
            expr.query().accept(*this);
            visit_order_by_start();
            visit_tuple_elements(expr.orders(), std::index_sequence_for<O...>{});
            visit_order_by_end();
        }

        template <typename Q>
        void visit(LimitExpr<Q>&& expr) {
            std::move(expr).query().accept(*this);
            visit_limit_impl(expr.count(), expr.offset());
        }

        template <typename Q>
        void visit(const LimitExpr<Q>& expr) {
            expr.query().accept(*this);
            visit_limit_impl(expr.count(), expr.offset());
        }

        template <typename Query, typename Condition>
        void visit(JoinExpr<Query, Condition>&& expr) {
            std::move(expr).query().accept(*this);
            visit_join_start(expr.type());
            visit_table_impl(std::move(expr).joined_table());
            visit_alias_impl(expr.alias());
            visit_join_on();
            std::move(expr).on_condition().accept(*this);
            visit_join_end();
        }

        template <typename Query, typename Condition>
        void visit(const JoinExpr<Query, Condition>& expr) {
            expr.query().accept(*this);
            visit_join_start(expr.type());
            visit_table_impl(expr.joined_table());
            visit_alias_impl(expr.alias());
            visit_join_on();
            expr.on_condition().accept(*this);
            visit_join_end();
        }

        // DML operations - perfect forwarding
        void visit(InsertExpr&& expr) {
            visit_insert_start();
            visit_table_impl(std::move(expr).table());
            visit_insert_columns(std::move(expr).columns());
            visit_insert_values(std::move(expr).rows());
            visit_insert_end();
        }

        void visit(const InsertExpr& expr) {
            visit_insert_start();
            visit_table_impl(expr.table());
            visit_insert_columns(expr.columns());
            visit_insert_values(expr.rows());
            visit_insert_end();
        }

        void visit(UpdateExpr&& expr) {
            visit_update_start();
            visit_table_impl(std::move(expr).table());
            visit_update_set(std::move(expr).assignments());
            visit_update_end();
        }

        void visit(const UpdateExpr& expr) {
            visit_update_start();
            visit_table_impl(expr.table());
            visit_update_set(expr.assignments());
            visit_update_end();
        }

        template <typename C>
        void visit(UpdateWhereExpr<C>&& expr) {
            std::move(expr).update().accept(*this);
            visit_where_start();
            std::move(expr).condition().accept(*this);
            visit_where_end();
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
        void visit(DeleteWhereExpr<C>&& expr) {
            std::move(expr).del().accept(*this);
            visit_where_start();
            std::move(expr).condition().accept(*this);
            visit_where_end();
        }

        template <typename C>
        void visit(const DeleteWhereExpr<C>& expr) {
            expr.del().accept(*this);
            visit_where_start();
            expr.condition().accept(*this);
            visit_where_end();
        }

        // Set operations - perfect forwarding
        template <typename L, typename R>
        void visit(SetOpExpr<L, R>&& expr) {
            std::move(expr).left().accept(*this);
            visit_set_op_impl(expr.op());
            std::move(expr).right().accept(*this);
        }

        template <typename L, typename R>
        void visit(const SetOpExpr<L, R>& expr) {
            expr.left().accept(*this);
            visit_set_op_impl(expr.op());
            expr.right().accept(*this);
        }

        // Case expressions - perfect forwarding
        template <typename... WhenClauses>
        void visit(CaseExpr<WhenClauses...>&& expr) {
            visit_case_start();
            std::apply([this]<typename... WhenClauseT>(WhenClauseT&&... when_clauses) {
                (..., (visit_when_start(),
                       std::forward<WhenClauseT>(when_clauses).condition.accept(*this),
                       visit_when_then(),
                       std::forward<WhenClauseT>(when_clauses).value.accept(*this),
                       visit_when_end()));
            }, std::move(expr).when_clauses());
            visit_case_end();
        }

        template <typename... WhenClauses>
        void visit(const CaseExpr<WhenClauses...>& expr) {
            visit_case_start();
            std::apply([this](const auto&... when_clauses) {
                (..., (visit_when_start(),
                       when_clauses.condition.accept(*this),
                       visit_when_then(),
                       when_clauses.value.accept(*this),
                       visit_when_end()));
            }, expr.when_clauses());
            visit_case_end();
        }

        template <typename ElseExpr, typename... WhenClauses>
        void visit(CaseExprWithElse<ElseExpr, WhenClauses...>&& expr) {
            visit_case_start();
            std::apply([this]<typename... WhenClauseT>(WhenClauseT&&... when_clauses) {
                (..., (visit_when_start(),
                       std::forward<WhenClauseT>(when_clauses).condition.accept(*this),
                       visit_when_then(),
                       std::forward<WhenClauseT>(when_clauses).value.accept(*this),
                       visit_when_end()));
            }, std::move(expr).when_clauses());
            visit_else_start();
            std::move(expr).else_clause().accept(*this);
            visit_else_end();
            visit_case_end();
        }

        template <typename ElseExpr, typename... WhenClauses>
        void visit(const CaseExprWithElse<ElseExpr, WhenClauses...>& expr) {
            visit_case_start();
            std::apply([this](const auto&... when_clauses) {
                (..., (visit_when_start(),
                       when_clauses.condition.accept(*this),
                       visit_when_then(),
                       when_clauses.value.accept(*this),
                       visit_when_end()));
            }, expr.when_clauses());
            visit_else_start();
            expr.else_clause().accept(*this);
            visit_else_end();
            visit_case_end();
        }

        // CTE expressions - perfect forwarding
        template <typename Query>
        void visit(CteExpr<Query>&& expr) {
            visit_cte_start(expr.recursive());
            visit_cte_name_impl(expr.name());
            visit_cte_as_start();
            std::move(expr).query().accept(*this);
            visit_cte_as_end();
            visit_cte_end();
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
        // Virtual interface methods - now with move support for appropriate parameters
        virtual void visit_table_column_impl(const FieldSchema* schema,
                                             const std::shared_ptr<std::string>& table,
                                             const std::optional<std::string>& alias) = 0;
        virtual void visit_dynamic_column_impl(const std::string& name,
                                               const std::optional<std::string>& table) = 0;
        virtual void visit_value_impl(const FieldValue& value) = 0;
        virtual void visit_value_impl(FieldValue&& value) = 0;
        virtual void visit_null_impl() = 0;

        virtual void visit_all_columns_impl(const std::shared_ptr<std::string>& table) = 0;

        virtual void visit_table_impl(const TableSchemaPtr& table) = 0;
        virtual void visit_table_impl(std::string_view table_name) = 0;
        virtual void visit_table_impl(const std::shared_ptr<std::string>& table) = 0;

        virtual void visit_alias_impl(const std::optional<std::string>& alias) = 0;
        virtual void visit_alias_impl(std::string_view alias) = 0;

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

        // DML - now with move support
        virtual void visit_insert_start() = 0;
        virtual void visit_insert_columns(const std::vector<std::string>& columns) = 0;
        virtual void visit_insert_columns(std::vector<std::string>&& columns) = 0;
        virtual void visit_insert_values(const std::vector<std::vector<FieldValue>>& rows) = 0;
        virtual void visit_insert_values(std::vector<std::vector<FieldValue>>&& rows) = 0;
        virtual void visit_insert_end() = 0;

        virtual void visit_update_start() = 0;
        virtual void visit_update_set(const std::vector<std::pair<std::string, FieldValue>>& assignments) = 0;
        virtual void visit_update_set(std::vector<std::pair<std::string, FieldValue>>&& assignments) = 0;
        virtual void visit_update_end() = 0;

        virtual void visit_delete_start() = 0;
        virtual void visit_delete_end() = 0;

        // Set operations
        virtual void visit_set_op_impl(SetOperation op) = 0;

        // Case/When/Else
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
        // Helper to visit tuple elements with perfect forwarding
        template <typename Tuple, std::size_t... Is>
        void visit_tuple_elements(Tuple&& t, std::index_sequence<Is...>) {
            bool first = true;
            ((visit_tuple_element(std::get<Is>(std::forward<Tuple>(t)), first)), ...);
        }

        template <typename T>
        void visit_tuple_element(T&& elem, bool& first) {
            if (!first) {
                visit_column_separator();
            }
            first = false;
            visit(std::forward<T>(elem));
        }
    };
}

#include "../source/query_visitor.inl"
