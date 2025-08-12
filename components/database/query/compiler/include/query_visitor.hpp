#pragma once

#include "db_core_fwd.hpp"
#include "gears_utils.hpp"
#include "query_expressions.hpp"

namespace demiplane::db {
    class QueryVisitor {
    public:
        virtual ~QueryVisitor() = default;

        // Column and literals - using deducing this
        template <typename T, typename Self, IsColumn ColumnTRef>
            requires std::same_as<std::remove_cvref_t<ColumnTRef>, Column<T>>
        void visit(this Self&& self, ColumnTRef&& col) {
            self.visit_column_impl(std::forward<ColumnTRef>(col).schema(),
                                   std::forward<ColumnTRef>(col).table(),
                                   std::forward<ColumnTRef>(col).alias());
        }

        template <typename T, typename Self, IsLiteral LiteralTRef>
            requires std::same_as<std::remove_cvref_t<LiteralTRef>, Literal<T>>
        void visit(this Self&& self, LiteralTRef&& lit) {
            self.visit_literal_impl(FieldValue(std::forward<LiteralTRef>(lit).value()));
            self.visit_alias_impl(std::forward<LiteralTRef>(lit).alias());
        }

        template <typename Self, typename NullLiteralRef>
            requires std::same_as<std::remove_cvref_t<NullLiteralRef>, NullLiteral>
        void visit(this Self&& self, NullLiteralRef&& null) {
            gears::unused_value(null);
            self.visit_null_impl();
        }

        template <typename Self, IsAllColumns AllColumnsRef>
        void visit(this Self&& self, AllColumns&& all) {
            self.visit_all_columns_impl(std::forward<AllColumnsRef>(all).table());
        }

        // Expressions - using deducing this
        template <typename L, typename R, IsOperator Op, IsBinaryOperator BinaryOperatorTRef, typename Self>
            requires std::same_as<std::remove_cvref_t<BinaryOperatorTRef>, BinaryExpr<L, R, Op>>
        void visit(this Self&& self, BinaryOperatorTRef&& expr) {
            self.visit_binary_expr_start();
            std::forward<BinaryOperatorTRef>(expr).left().accept(self);
            self.visit_binary_op_impl(Op{});
            std::forward<BinaryOperatorTRef>(expr).right().accept(self);
            self.visit_binary_expr_end();
        }

        template <typename O, IsOperator Op, IsUnaryOperator UnaryOperatorRef, typename Self>
            requires std::same_as<std::remove_cvref_t<UnaryOperatorRef>, UnaryExpr<O, Op>>
        void visit(this Self&& self, UnaryOperatorRef&& expr) {
            self.visit_unary_expr_start();
            self.visit_unary_op_impl(Op{});
            self.visit(std::forward<UnaryOperatorRef>(expr).operand());
            self.visit_unary_expr_end();
        }

        template <typename O, typename L, typename U, IsBetweenExpr BetweenExprRef, typename Self>
            requires std::same_as<std::remove_cvref_t<BetweenExprRef>, BetweenExpr<O, L, U>>
        void visit(this Self&& self, BetweenExprRef&& expr) {
            std::forward<BetweenExprRef>(expr).operand().accept(self);
            self.visit_between_impl();
            std::forward<BetweenExprRef>(expr).lower().accept(self);
            self.visit_and_impl();
            std::forward<BetweenExprRef>(expr).upper().accept(self);
        }

        template <typename O, typename... Values, IsInListExpr InListExprRef, typename Self>
            requires std::same_as<std::remove_cvref_t<InListExprRef>, InListExpr<O, Values...>>
        void visit(this Self&& self, InListExprRef&& expr) {
            std::forward<InListExprRef>(expr).operand().accept(self);
            self.visit_in_list_start();
            self.visit_tuple_elements(std::forward<InListExprRef>(expr).values(),
                                      std::index_sequence_for<Values...>{});
            self.visit_in_list_end();
        }

        template <typename Q, IsSubquery SubqueryRef, typename Self>
            requires std::same_as<std::remove_cvref_t<SubqueryRef>, Subquery<Q>>
        void visit(this Self&& self, SubqueryRef&& sq) {
            self.visit_subquery_start();
            std::forward<SubqueryRef>(sq).query().accept(self);
            self.visit_subquery_end();
            if (sq.alias()) {
                self.visit_alias_impl(std::forward<SubqueryRef>(sq).alias());
            }
        }

        template <typename Q, IsExistExpr ExistExprRef, typename Self>
            requires std::same_as<std::remove_cvref_t<ExistExprRef>, ExistsExpr<Q>>
        void visit(this Self&& self, ExistExprRef&& expr) {
            self.visit_exists_start();
            std::forward<ExistExprRef>(expr).query().accept(self);
            self.visit_exists_end();
        }

        // Aggregate functions - using deducing this
        template <typename T, IsCountExpr CountExprRef, typename Self>
            requires std::same_as<std::remove_cvref_t<CountExprRef>, CountExpr<T>>
        void visit(this Self&& self, CountExprRef&& expr) {
            self.visit_count_impl(expr.distinct());
            if (expr.column().schema()) {
                std::forward<CountExprRef>(expr).column().accept(self);
            }
            self.visit_aggregate_end(std::forward<CountExprRef>(expr).alias());
        }

        template <typename T, IsSumExpr SumExprRef, typename Self>
            requires std::same_as<std::remove_cvref_t<SumExprRef>, SumExpr<T>>
        void visit(this Self&& self, SumExprRef&& expr) {
            self.visit_sum_impl();
            std::forward<SumExprRef>(expr).column().accept(self);
            self.visit_aggregate_end(std::forward<SumExprRef>(expr).alias());
        }

        template <typename T, IsAvgExpr AvgExprRef, typename Self>
            requires std::same_as<std::remove_cvref_t<AvgExprRef>, AvgExpr<T>>
        void visit(this Self&& self, AvgExprRef&& expr) {
            self.visit_avg_impl();
            std::forward<AvgExprRef>(expr).column().accept(self);
            self.visit_aggregate_end(std::forward<AvgExprRef>(expr).alias());
        }

        template <typename T, IsMaxExpr MaxExprRef, typename Self>
            requires std::same_as<std::remove_cvref_t<MaxExprRef>, MaxExpr<T>>
        void visit(this Self&& self, MaxExprRef&& expr) {
            self.visit_max_impl();
            std::forward<MaxExprRef>(expr).column().accept(self);
            self.visit_aggregate_end(std::forward<MaxExprRef>(expr).alias());
        }

        template <typename T, IsMinExpr MinExprRef, typename Self>
            requires std::same_as<std::remove_cvref_t<MinExprRef>, MinExpr<T>>
        void visit(this Self&& self, MinExprRef&& expr) {
            self.visit_min_impl();
            std::forward<MinExprRef>(expr).column().accept(self);
            self.visit_aggregate_end(std::forward<MinExprRef>(expr).alias());
        }


        template <typename T, IsOrderBy OrderByRef, typename Self>
            requires std::same_as<std::remove_cvref_t<OrderByRef>, OrderBy<T>>
        void visit(this Self&& self, OrderByRef&& order) {
            std::forward<OrderByRef>(order).column().accept(self);
            self.visit_order_direction_impl(order.direction());
        }

        // Query builders - using deducing this
        template <typename... Columns, IsSelectExpr SelectExprRef, typename Self>
            requires std::same_as<std::remove_cvref_t<SelectExprRef>, SelectExpr<Columns...>>
        void visit(this Self&& self, SelectExprRef&& expr) {
            self.visit_select_start(expr.distinct());
            self.visit_tuple_elements(std::forward<SelectExprRef>(expr).columns(),
                                      std::index_sequence_for<Columns...>{});
            self.visit_select_end();
        }

        template <typename SelectQuery, IsFromExpr FromTableExprRef, typename Self>
            requires std::same_as<std::remove_cvref_t<FromTableExprRef>, FromTableExpr<SelectQuery>>
        void visit(this Self&& self, FromTableExprRef&& expr) {
            std::forward<FromTableExprRef>(expr).select().accept(self);
            self.visit_from_start();
            self.visit_table_impl(std::forward<FromTableExprRef>(expr).table());
            self.visit_alias_impl(std::forward<FromTableExprRef>(expr).alias());
            self.visit_from_end();
        }

        template <typename SelectQuery, typename FromAnotherQuery, IsFromExpr FromQueryExprRef, typename Self>
            requires std::same_as<std::remove_cvref_t<FromQueryExprRef>, FromQueryExpr<SelectQuery, FromAnotherQuery>>
        void visit(this Self&& self, FromQueryExprRef&& expr) {
            std::forward<FromQueryExprRef>(expr).select().accept(self);
            self.visit_from_start();
            self.visit(std::forward<FromQueryExprRef>(expr).query());
            self.visit_from_end();
        }

        template <typename Q, typename C, IsWhereExpr WhereExprRef, typename Self>
            requires std::same_as<std::remove_cvref_t<WhereExprRef>, WhereExpr<Q, C>>
        void visit(this Self&& self, WhereExprRef&& expr) {
            std::forward<WhereExprRef>(expr).query().accept(self);
            self.visit_where_start();
            std::forward<WhereExprRef>(expr).condition().accept(self);
            self.visit_where_end();
        }

        template <typename PreGroupQuery, typename... Columns, IsGroupByExpr GroupByColumnExprRef, typename Self>
            requires std::same_as<std::remove_cvref_t<GroupByColumnExprRef>, GroupByColumnExpr<
                                      PreGroupQuery, Columns...>>
        void visit(this Self&& self, GroupByColumnExprRef&& expr) {
            std::forward<GroupByColumnExprRef>(expr).query().accept(self);
            self.visit_group_by_start();
            self.visit_tuple_elements(std::forward<GroupByColumnExprRef>(expr).columns(),
                                      std::index_sequence_for<Columns...>{});
            self.visit_group_by_end();
        }

        template <typename PreGroupQuery, typename Criteria, IsGroupByExpr GroupByQueryExprRef, typename Self>
            requires std::same_as<std::remove_cvref_t<GroupByQueryExprRef>, GroupByQueryExpr<PreGroupQuery, Criteria>>
        void visit(this Self&& self, GroupByQueryExprRef&& expr) {
            std::forward<GroupByQueryExprRef>(expr).query().accept(self);
            self.visit_group_by_start();
            self.visit(std::forward<GroupByQueryExprRef>(expr).criteria());
            self.visit_group_by_end();
        }

        template <typename Q, typename C, IsHavingExpr HavingExprRef, typename Self>
            requires std::same_as<std::remove_cvref_t<HavingExprRef>, HavingExpr<Q, C>>
        void visit(this Self&& self, HavingExprRef&& expr) {
            std::forward<HavingExprRef>(expr).query().accept(self);
            self.visit_having_start();
            std::forward<HavingExprRef>(expr).condition().accept(self);
            self.visit_having_end();
        }

        template <typename Q, typename... O, IsOrderByExpr OrderByExprRef, typename Self>
            requires std::same_as<std::remove_cvref_t<OrderByExprRef>, OrderByExpr<Q, O...>>
        void visit(this Self&& self, OrderByExprRef&& expr) {
            std::forward<OrderByExprRef>(expr).query().accept(self);
            self.visit_order_by_start();
            self.visit_tuple_elements(std::forward<OrderByExprRef>(expr).orders(),
                                      std::index_sequence_for<O...>{});
            self.visit_order_by_end();
        }

        template <typename Q, IsLimitExpr LimitExprRef, typename Self>
            requires std::same_as<std::remove_cvref_t<LimitExprRef>, LimitExpr<Q>>
        void visit(this Self&& self, LimitExprRef&& expr) {
            std::forward<LimitExprRef>(expr).query().accept(self);
            self.visit_limit_impl(expr.count(), expr.offset());
        }

        template <typename Q, typename J, typename C, IsJoinExpr JoinExprRef, typename Self>
            requires std::same_as<std::remove_cvref_t<JoinExprRef>, JoinExpr<Q, J, C>>
        void visit(this Self&& self, JoinExprRef&& expr) {
            std::forward<JoinExprRef>(expr).query().accept(self);
            self.visit_join_start(expr.type());
            self.visit_table_impl(std::forward<JoinExprRef>(expr).joined_table());
            self.visit_alias_impl(std::forward<JoinExprRef>(expr).joined_alias());
            self.visit_join_on();
            std::forward<JoinExprRef>(expr).on_condition().accept(self);
            self.visit_join_end();
        }

        // DML operations - using deducing this
        template <typename Self, IsInsertExpr InsertExprRef>
            requires std::same_as<std::remove_cvref_t<InsertExprRef>, InsertExpr>
        void visit(this Self&& self, InsertExprRef&& expr) {
            self.visit_insert_start();
            self.visit_table_impl(std::forward<InsertExprRef>(expr).table());
            self.visit_insert_columns(std::forward<InsertExprRef>(expr).columns());
            self.visit_insert_values(std::forward<InsertExprRef>(expr).rows());
            self.visit_insert_end();
        }

        template <typename Self, IsUpdateExpr UpdateExprRef>
            requires std::same_as<std::remove_cvref_t<UpdateExprRef>, UpdateExpr>
        void visit(this Self&& self, UpdateExprRef&& expr) {
            self.visit_update_start();
            self.visit_table_impl(std::forward<UpdateExprRef>(expr).table());
            self.visit_update_set(std::forward<UpdateExprRef>(expr).assignments());
            self.visit_update_end();
        }

        template <typename C, IsUpdateExpr UpdateWhereExprRef, typename Self>
            requires std::same_as<std::remove_cvref_t<UpdateWhereExprRef>, UpdateWhereExpr<C>>
        void visit(this Self&& self, UpdateWhereExprRef&& expr) {
            std::forward<UpdateWhereExprRef>(expr).update().accept(self);
            self.visit_where_start();
            std::forward<UpdateWhereExprRef>(expr).condition().accept(self);
            self.visit_where_end();
        }

        template <typename Self, IsDeleteExpr DeleteExprRef>
            requires std::same_as<std::remove_cvref_t<DeleteExprRef>, DeleteExpr>
        void visit(this Self&& self, DeleteExprRef&& expr) {
            self.visit_delete_start();
            self.visit_table_impl(std::forward<DeleteExprRef>(expr).table());
            self.visit_delete_end();
        }

        template <typename C, IsDeleteExpr DeleteWhereExprRef, typename Self>
            requires std::same_as<std::remove_cvref_t<DeleteWhereExprRef>, DeleteWhereExpr<C>>
        void visit(this Self&& self, DeleteWhereExprRef&& expr) {
            std::forward<DeleteWhereExprRef>(expr).del().accept(self);
            self.visit_where_start();
            std::forward<DeleteWhereExprRef>(expr).condition().accept(self);
            self.visit_where_end();
        }

        // Set operations - using deducing this
        template <typename L, typename R, IsSetOpExpr SetOpExprRef, typename Self>
            requires std::same_as<std::remove_cvref_t<SetOpExprRef>, SetOpExpr<L, R>>
        void visit(this Self&& self, SetOpExprRef&& expr) {
            std::forward<SetOpExprRef>(expr).left().accept(self);
            self.visit_set_op_impl(expr.op());
            std::forward<SetOpExprRef>(expr).right().accept(self);
        }

        // Case expressions - using deducing this
        template <typename... WhenClauses, IsCaseExpr CaseExprRef, typename Self>
            requires std::same_as<std::remove_cvref_t<CaseExprRef>, CaseExpr<WhenClauses...>>
        void visit(this Self&& self, CaseExprRef&& expr) {
            self.visit_case_start();
            std::apply([&self](auto&&... when_clauses) {
                (..., (self.visit_when_start(),
                       std::forward<decltype(when_clauses)>(when_clauses).condition.accept(self),
                       self.visit_when_then(),
                       std::forward<decltype(when_clauses)>(when_clauses).value.accept(self),
                       self.visit_when_end()));
            }, std::forward<CaseExprRef>(expr).when_clauses());
            self.visit_case_end();
        }

        template <typename ElseExpr, typename... WhenClauses, IsCaseExpr CaseExprWithElseRef, typename Self>
            requires std::same_as<std::remove_cvref_t<CaseExprWithElseRef>, CaseExprWithElse<ElseExpr, WhenClauses...>>
        void visit(this Self&& self, CaseExprWithElseRef&& expr) {
            self.visit_case_start();
            std::apply([&self](auto&&... when_clauses) {
                (..., (self.visit_when_start(),
                       std::forward<decltype(when_clauses)>(when_clauses).condition.accept(self),
                       self.visit_when_then(),
                       std::forward<decltype(when_clauses)>(when_clauses).value.accept(self),
                       self.visit_when_end()));
            }, std::forward<CaseExprWithElseRef>(expr).when_clauses());
            self.visit_else_start();
            std::forward<CaseExprWithElseRef>(expr).else_clause().accept(self);
            self.visit_else_end();
            self.visit_case_end();
        }

        // CTE expressions - using deducing this
        template <typename Query, IsCteExpr CteExprRef, typename Self>
            requires std::same_as<std::remove_cvref_t<CteExprRef>, CteExpr<Query>>
        void visit(this Self&& self, CteExprRef&& expr) {
            self.visit_cte_start(expr.recursive());
            self.visit_cte_name_impl(expr.name());
            self.visit_cte_as_start();
            std::forward<CteExprRef>(expr).query().accept(self);
            self.visit_cte_as_end();
            self.visit_cte_end();
        }

    protected:
        // Virtual interface methods
        virtual void visit_column_impl(const FieldSchema* schema,
                                       std::shared_ptr<std::string>&& table,
                                       std::optional<std::string>&& alias) = 0;
        virtual void visit_column_impl(const FieldSchema* schema,
                                       const std::shared_ptr<std::string>& table,
                                       const std::optional<std::string>& alias) = 0;
        virtual void visit_literal_impl(const FieldValue& value) = 0;
        virtual void visit_literal_impl(FieldValue&& value) = 0;
        virtual void visit_null_impl() = 0;

        virtual void visit_all_columns_impl(const std::shared_ptr<std::string>& table) = 0;
        virtual void visit_all_columns_impl(std::shared_ptr<std::string>&& table) = 0;

        virtual void visit_parameter_impl(std::size_t index) = 0;

        virtual void visit_table_impl(const TableSchemaPtr& table) = 0;
        virtual void visit_table_impl(TableSchemaPtr&& table) = 0;

        virtual void visit_table_spec_impl(const std::shared_ptr<std::string>& table) = 0;
        virtual void visit_table_spec_impl(std::shared_ptr<std::string>&& table) = 0;

        virtual void visit_alias_impl(const std::optional<std::string>& alias) = 0;
        virtual void visit_alias_impl(std::optional<std::string>&& alias) = 0;

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
        virtual void visit_aggregate_end(std::optional<std::string>&& alias) = 0;
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
        template <typename Tuple, std::size_t... Is, typename Self>
        void visit_tuple_elements(this Self&& self, Tuple&& t, std::index_sequence<Is...>) {
            bool first = true;
            ((self.visit_tuple_element(std::get<Is>(std::forward<Tuple>(t)), first)), ...);
        }

        template <typename T, typename Self>
        void visit_tuple_element(this Self&& self, T&& elem, bool& first) {
            if (!first) {
                self.visit_column_separator();
            }
            first = false;
            self.visit(std::forward<T>(elem));
        }
    };

    // Update accept methods in Expression classes to use deducing this
    template <typename T>
    void Column<T>::accept(this auto&& self, QueryVisitor& visitor) {
        visitor.visit(std::forward<decltype(self)>(self));
    }

    // Specialization for Column<void>
    void Column<void>::accept(this auto&& self, QueryVisitor& visitor) {
        visitor.visit(std::forward<decltype(self)>(self));
    }

    template <typename Derived>
    void Expression<Derived>::accept(this auto&& self, QueryVisitor& visitor) {
        visitor.visit(std::forward<decltype(self)>(self).self());
    }

    template <typename T>
    void Literal<T>::accept(this auto&& self, QueryVisitor& visitor) {
        visitor.visit(std::forward<decltype(self)>(self));
    }
}
