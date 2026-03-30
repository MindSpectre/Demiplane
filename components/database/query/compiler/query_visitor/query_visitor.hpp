#pragma once

#include <db_core_objects.hpp>
#include <db_typed_column.hpp>

#include "query_expressions.hpp"

namespace demiplane::db {
    template <typename Derived>
    class QueryVisitor {
        [[nodiscard]] constexpr Derived& derived() noexcept {
            return static_cast<Derived&>(*this);
        }
        [[nodiscard]] constexpr const Derived& derived() const noexcept {
            return static_cast<const Derived&>(*this);
        }

    public:
        constexpr auto visit(const Column& col) {
            derived().visit_column_impl(col.name(), col.table_name(), col.alias());
            return derived().no_params();
        }

        template <IsTypedColumn ColT>
        constexpr auto visit(const ColT& col) {
            derived().visit_column_impl(col.name(), col.table_name(), col.alias());
            return derived().no_params();
        }

        template <typename T>
        constexpr auto visit(Literal<T>&& lit) {
            // Value is consumed by exactly one consumer depending on mode:
            // - Tuple mode: capture_param owns the value (visit_value_impl just writes $N)
            // - Sink/Inline mode: visit_value_impl consumes the value
            using CaptureT          = decltype(derived().capture_param(lit.value));
            constexpr bool captures = std::tuple_size_v<CaptureT> > 0;

            if constexpr (std::is_same_v<T, FieldValue> && !captures) {
                derived().visit_value_impl(std::move(lit.value));
            } else if constexpr (std::is_same_v<T, FieldValue>) {
                derived().visit_value_impl(lit.value);
            } else {
                derived().visit_value_impl(FieldValue{lit.value});
            }
            derived().visit_alias_impl(std::move(lit.alias));

            if constexpr (captures) {
                return derived().capture_param(std::move(lit.value));
            } else {
                return derived().no_params();
            }
        }

        template <typename T>
        constexpr auto visit(const Literal<T>& lit) {
            if constexpr (std::is_same_v<T, FieldValue>) {
                derived().visit_value_impl(lit.value);
            } else {
                derived().visit_value_impl(FieldValue{lit.value});
            }
            derived().visit_alias_impl(lit.alias);
            return derived().capture_param(lit.value);
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

        template <typename L, typename R, IsOperator Op>
        constexpr auto visit(BinaryExpr<L, R, Op>&& expr) {
            auto&& [left, right] = std::move(expr).decompose();
            derived().visit_binary_expr_start();
            auto lp = std::move(left).accept(derived());
            derived().visit_binary_op_impl(Op{});
            auto rp = std::move(right).accept(derived());
            derived().visit_binary_expr_end();
            return derived().cat_params(std::move(lp), std::move(rp));
        }

        template <typename L, typename R, IsOperator Op>
        constexpr auto visit(const BinaryExpr<L, R, Op>& expr) {
            auto&& [left, right] = expr.decompose();
            derived().visit_binary_expr_start();
            auto lp = left.accept(derived());
            derived().visit_binary_op_impl(Op{});
            auto rp = right.accept(derived());
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
            auto&& [operand, lower, upper] = std::move(expr).decompose();
            auto op                        = std::move(operand).accept(derived());
            derived().visit_between_impl();
            auto lp = std::move(lower).accept(derived());
            derived().visit_and_impl();
            auto up = std::move(upper).accept(derived());
            return derived().cat_params(std::move(op), std::move(lp), std::move(up));
        }

        template <typename O, typename L, typename U>
        constexpr auto visit(const BetweenExpr<O, L, U>& expr) {
            auto&& [operand, lower, upper] = expr.decompose();
            auto op                        = operand.accept(derived());
            derived().visit_between_impl();
            auto lp = lower.accept(derived());
            derived().visit_and_impl();
            auto up = upper.accept(derived());
            return derived().cat_params(std::move(op), std::move(lp), std::move(up));
        }

        template <typename O, typename... Values>
        constexpr auto visit(InListExpr<O, Values...>&& expr) {
            auto&& [operand, values] = std::move(expr).decompose();
            auto op                  = std::move(operand).accept(derived());
            derived().visit_in_list_start();
            auto vp = visit_tuple_elements(std::move(values), std::index_sequence_for<Values...>{});
            derived().visit_in_list_end();
            return derived().cat_params(std::move(op), std::move(vp));
        }

        template <typename O, typename... Values>
        constexpr auto visit(const InListExpr<O, Values...>& expr) {
            auto&& [operand, values] = expr.decompose();
            auto op                  = operand.accept(derived());
            derived().visit_in_list_start();
            auto vp = visit_tuple_elements(values, std::index_sequence_for<Values...>{});
            derived().visit_in_list_end();
            return derived().cat_params(std::move(op), std::move(vp));
        }

        template <typename Q>
        constexpr auto visit(Subquery<Q>&& sq) {
            auto&& [query, alias] = std::move(sq).decompose();
            derived().visit_subquery_start();
            auto qp = std::move(query).accept(derived());
            derived().visit_subquery_end();
            derived().visit_alias_impl(alias);
            return qp;
        }

        template <typename Q>
        constexpr auto visit(const Subquery<Q>& sq) {
            auto&& [query, alias] = sq.decompose();
            derived().visit_subquery_start();
            auto qp = query.accept(derived());
            derived().visit_subquery_end();
            derived().visit_alias_impl(alias);
            return qp;
        }

        template <typename Q>
        constexpr auto visit(ExistsExpr<Q>&& expr) {
            auto&& [query] = std::move(expr).decompose();
            derived().visit_exists_start();
            auto qp = std::move(query).accept(derived());
            derived().visit_exists_end();
            return qp;
        }

        template <typename Q>
        constexpr auto visit(const ExistsExpr<Q>& expr) {
            auto&& [query] = expr.decompose();
            derived().visit_exists_start();
            auto qp = query.accept(derived());
            derived().visit_exists_end();
            return qp;
        }

        template <IsColumnLike ColT>
        constexpr auto visit(const CountExpr<ColT>& expr) {
            derived().visit_count_impl(expr.distinct());
            if constexpr (IsAllColumns<ColT>) {
                derived().visit_all_columns_impl("");
            } else {
                expr.column.accept(derived());
            }
            derived().visit_aggregate_end(expr.alias);
            return derived().no_params();
        }

        template <IsColumnLike ColT>
        constexpr auto visit(const SumExpr<ColT>& expr) {
            derived().visit_sum_impl();
            expr.column.accept(derived());
            derived().visit_aggregate_end(expr.alias);
            return derived().no_params();
        }

        template <IsColumnLike ColT>
        constexpr auto visit(const AvgExpr<ColT>& expr) {
            derived().visit_avg_impl();
            expr.column.accept(derived());
            derived().visit_aggregate_end(expr.alias);
            return derived().no_params();
        }

        template <IsColumnLike ColT>
        constexpr auto visit(const MaxExpr<ColT>& expr) {
            derived().visit_max_impl();
            expr.column.accept(derived());
            derived().visit_aggregate_end(expr.alias);
            return derived().no_params();
        }

        template <IsColumnLike ColT>
        constexpr auto visit(const MinExpr<ColT>& expr) {
            derived().visit_min_impl();
            expr.column.accept(derived());
            derived().visit_aggregate_end(expr.alias);
            return derived().no_params();
        }

        template <IsColumnLike ColT>
        constexpr auto visit(const OrderBy<ColT>& order) {
            order.column.accept(derived());
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
            auto&& [select, table, alias] = std::move(expr).decompose();
            auto sp                       = std::move(select).accept(derived());
            derived().visit_from_start();
            derived().visit_table_impl(table);
            derived().visit_alias_impl(alias);
            derived().visit_from_end();
            return sp;
        }

        template <typename SelectQuery, typename IsTable>
        constexpr auto visit(const FromTableExpr<SelectQuery, IsTable>& expr) {
            auto&& [select, table, alias] = expr.decompose();
            auto sp                       = select.accept(derived());
            derived().visit_from_start();
            derived().visit_table_impl(table);
            derived().visit_alias_impl(alias);
            derived().visit_from_end();
            return sp;
        }

        template <IsSelectExpr SelectQuery, IsCteExpr CteQuery>
        constexpr auto visit(FromCteExpr<SelectQuery, CteQuery>&& expr) {
            auto&& [select, cte_query] = std::move(expr).decompose();
            auto cte_name              = cte_query.name();
            auto cp                    = derived().visit(std::move(cte_query));
            auto sp                    = std::move(select).accept(derived());
            derived().visit_from_start();
            derived().visit_table_impl(cte_name);
            derived().visit_from_end();
            return derived().cat_params(std::move(cp), std::move(sp));
        }

        template <IsSelectExpr SelectQuery, IsCteExpr CteQuery>
        constexpr auto visit(const FromCteExpr<SelectQuery, CteQuery>& expr) {
            auto&& [select, cte_query] = expr.decompose();
            auto cp                    = derived().visit(cte_query);
            auto sp                    = select.accept(derived());
            derived().visit_from_start();
            derived().visit_table_impl(cte_query.name());
            derived().visit_from_end();
            return derived().cat_params(std::move(cp), std::move(sp));
        }

        template <typename Q, typename C>
        constexpr auto visit(WhereExpr<Q, C>&& expr) {
            auto&& [query, condition] = std::move(expr).decompose();
            auto qp                   = std::move(query).accept(derived());
            derived().visit_where_start();
            auto cp = std::move(condition).accept(derived());
            derived().visit_where_end();
            return derived().cat_params(std::move(qp), std::move(cp));
        }

        template <typename Q, typename C>
        constexpr auto visit(const WhereExpr<Q, C>& expr) {
            auto&& [query, condition] = expr.decompose();
            auto qp                   = query.accept(derived());
            derived().visit_where_start();
            auto cp = condition.accept(derived());
            derived().visit_where_end();
            return derived().cat_params(std::move(qp), std::move(cp));
        }

        template <typename PreGroupQuery, typename... Columns>
        constexpr auto visit(GroupByColumnExpr<PreGroupQuery, Columns...>&& expr) {
            auto&& [query, columns] = std::move(expr).decompose();
            auto qp                 = std::move(query).accept(derived());
            derived().visit_group_by_start();
            auto cp = visit_tuple_elements(std::move(columns), std::index_sequence_for<Columns...>{});
            derived().visit_group_by_end();
            return derived().cat_params(std::move(qp), std::move(cp));
        }

        template <typename PreGroupQuery, typename... Columns>
        constexpr auto visit(const GroupByColumnExpr<PreGroupQuery, Columns...>& expr) {
            auto&& [query, columns] = expr.decompose();
            auto qp                 = query.accept(derived());
            derived().visit_group_by_start();
            auto cp = visit_tuple_elements(columns, std::index_sequence_for<Columns...>{});
            derived().visit_group_by_end();
            return derived().cat_params(std::move(qp), std::move(cp));
        }

        template <typename PreGroupQuery, typename Criteria>
        constexpr auto visit(GroupByQueryExpr<PreGroupQuery, Criteria>&& expr) {
            auto&& [query, criteria] = std::move(expr).decompose();
            auto qp                  = std::move(query).accept(derived());
            derived().visit_group_by_start();
            auto cp = derived().visit(std::move(criteria));
            derived().visit_group_by_end();
            return derived().cat_params(std::move(qp), std::move(cp));
        }

        template <typename PreGroupQuery, typename Criteria>
        constexpr auto visit(const GroupByQueryExpr<PreGroupQuery, Criteria>& expr) {
            auto&& [query, criteria] = expr.decompose();
            auto qp                  = query.accept(derived());
            derived().visit_group_by_start();
            auto cp = derived().visit(criteria);
            derived().visit_group_by_end();
            return derived().cat_params(std::move(qp), std::move(cp));
        }

        template <typename Q, typename C>
        constexpr auto visit(HavingExpr<Q, C>&& expr) {
            auto&& [query, condition] = std::move(expr).decompose();
            auto qp                   = std::move(query).accept(derived());
            derived().visit_having_start();
            auto cp = std::move(condition).accept(derived());
            derived().visit_having_end();
            return derived().cat_params(std::move(qp), std::move(cp));
        }

        template <typename Q, typename C>
        constexpr auto visit(const HavingExpr<Q, C>& expr) {
            auto&& [query, condition] = expr.decompose();
            auto qp                   = query.accept(derived());
            derived().visit_having_start();
            auto cp = condition.accept(derived());
            derived().visit_having_end();
            return derived().cat_params(std::move(qp), std::move(cp));
        }

        template <typename Q, typename... O>
        constexpr auto visit(OrderByExpr<Q, O...>&& expr) {
            auto&& [query, orders] = std::move(expr).decompose();
            auto qp                = std::move(query).accept(derived());
            derived().visit_order_by_start();
            auto op = visit_tuple_elements(std::move(orders), std::index_sequence_for<O...>{});
            derived().visit_order_by_end();
            return derived().cat_params(std::move(qp), std::move(op));
        }

        template <typename Q, typename... O>
        constexpr auto visit(const OrderByExpr<Q, O...>& expr) {
            auto&& [query, orders] = expr.decompose();
            auto qp                = query.accept(derived());
            derived().visit_order_by_start();
            auto op = visit_tuple_elements(orders, std::index_sequence_for<O...>{});
            derived().visit_order_by_end();
            return derived().cat_params(std::move(qp), std::move(op));
        }

        template <typename Q>
        constexpr auto visit(LimitExpr<Q>&& expr) {
            auto&& [query, count, offset] = std::move(expr).decompose();
            auto qp                       = std::move(query).accept(derived());
            derived().visit_limit_impl(count, offset);
            return qp;
        }

        template <typename Q>
        constexpr auto visit(const LimitExpr<Q>& expr) {
            auto&& [query, count, offset] = expr.decompose();
            auto qp                       = query.accept(derived());
            derived().visit_limit_impl(count, offset);
            return qp;
        }

        template <typename Query, typename Condition, typename TableT>
        constexpr auto visit(JoinExpr<Query, Condition, TableT>&& expr) {
            auto&& [query, on_condition, table, type, alias] = std::move(expr).decompose();
            auto qp                                          = std::move(query).accept(derived());
            derived().visit_join_start(type);
            derived().visit_table_impl(table);
            derived().visit_alias_impl(alias);
            derived().visit_join_on();
            auto cp = std::move(on_condition).accept(derived());
            derived().visit_join_end();
            return derived().cat_params(std::move(qp), std::move(cp));
        }

        template <typename Query, typename Condition, typename TableT>
        constexpr auto visit(const JoinExpr<Query, Condition, TableT>& expr) {
            auto&& [query, on_condition, table, type, alias] = expr.decompose();
            auto qp                                          = query.accept(derived());
            derived().visit_join_start(type);
            derived().visit_table_impl(table);
            derived().visit_alias_impl(alias);
            derived().visit_join_on();
            auto cp = on_condition.accept(derived());
            derived().visit_join_end();
            return derived().cat_params(std::move(qp), std::move(cp));
        }

        template <IsTable TableT>
        constexpr auto visit(InsertExpr<TableT>&& expr) {
            auto&& [table, columns, rows] = std::move(expr).decompose();
            derived().visit_insert_start();
            derived().visit_table_impl(table);
            derived().visit_insert_columns(std::move(columns));
            derived().visit_insert_values(std::move(rows));
            derived().visit_insert_end();
            return derived().no_params();
        }

        template <IsTable TableT>
        constexpr auto visit(const InsertExpr<TableT>& expr) {
            auto&& [table, columns, rows] = expr.decompose();
            derived().visit_insert_start();
            derived().visit_table_impl(table);
            derived().visit_insert_columns(columns);
            derived().visit_insert_values(rows);
            derived().visit_insert_end();
            return derived().no_params();
        }

        template <IsTable TableT>
        constexpr auto visit(UpdateExpr<TableT>&& expr) {
            auto&& [table, assignments] = std::move(expr).decompose();
            derived().visit_update_start();
            derived().visit_table_impl(table);
            derived().visit_update_set(std::move(assignments));
            derived().visit_update_end();
            return derived().no_params();
        }

        template <IsTable TableT>
        constexpr auto visit(const UpdateExpr<TableT>& expr) {
            auto&& [table, assignments] = expr.decompose();
            derived().visit_update_start();
            derived().visit_table_impl(table);
            derived().visit_update_set(assignments);
            derived().visit_update_end();
            return derived().no_params();
        }

        template <IsTable TableT, IsCondition ConditionT>
        constexpr auto visit(UpdateWhereExpr<TableT, ConditionT>&& expr) {
            auto&& [update, condition] = std::move(expr).decompose();
            auto up                    = std::move(update).accept(derived());
            derived().visit_where_start();
            auto cp = std::move(condition).accept(derived());
            derived().visit_where_end();
            return derived().cat_params(std::move(up), std::move(cp));
        }

        template <IsTable TableT, IsCondition ConditionT>
        constexpr auto visit(const UpdateWhereExpr<TableT, ConditionT>& expr) {
            auto&& [update, condition] = expr.decompose();
            auto up                    = update.accept(derived());
            derived().visit_where_start();
            auto cp = condition.accept(derived());
            derived().visit_where_end();
            return derived().cat_params(std::move(up), std::move(cp));
        }

        template <typename T>
        constexpr auto visit(const DeleteExpr<T>& expr) {
            derived().visit_delete_start();
            derived().visit_table_impl(expr.table);
            derived().visit_delete_end();
            return derived().no_params();
        }

        template <typename T, typename C>
        constexpr auto visit(DeleteWhereExpr<T, C>&& expr) {
            auto&& [del, condition] = std::move(expr).decompose();
            auto dp                 = std::move(del).accept(derived());
            derived().visit_where_start();
            auto cp = std::move(condition).accept(derived());
            derived().visit_where_end();
            return derived().cat_params(std::move(dp), std::move(cp));
        }

        template <typename T, typename C>
        constexpr auto visit(const DeleteWhereExpr<T, C>& expr) {
            auto&& [del, condition] = expr.decompose();
            auto dp                 = del.accept(derived());
            derived().visit_where_start();
            auto cp = condition.accept(derived());
            derived().visit_where_end();
            return derived().cat_params(std::move(dp), std::move(cp));
        }

        // Set operations - perfect forwarding
        template <typename L, typename R>
        constexpr auto visit(SetOpExpr<L, R>&& expr) {
            auto&& [left, right, op] = std::move(expr).decompose();
            auto lp                  = std::move(left).accept(derived());
            derived().visit_set_op_impl(op);
            auto rp = std::move(right).accept(derived());
            return derived().cat_params(std::move(lp), std::move(rp));
        }

        template <typename L, typename R>
        constexpr auto visit(const SetOpExpr<L, R>& expr) {
            auto&& [left, right, op] = expr.decompose();
            auto lp                  = left.accept(derived());
            derived().visit_set_op_impl(op);
            auto rp = right.accept(derived());
            return derived().cat_params(std::move(lp), std::move(rp));
        }

        // Case expressions - perfect forwarding
        template <typename... WhenClauses>
        constexpr auto visit(CaseExpr<WhenClauses...>&& expr) {
            auto&& [when_clauses, alias] = std::move(expr).decompose();
            derived().visit_case_start();
            auto wp = visit_when_clauses(std::move(when_clauses), std::index_sequence_for<WhenClauses...>{});
            derived().visit_case_end();
            derived().visit_alias_impl(alias);
            return wp;
        }

        template <typename... WhenClauses>
        constexpr auto visit(const CaseExpr<WhenClauses...>& expr) {
            auto&& [when_clauses, alias] = expr.decompose();
            derived().visit_case_start();
            auto wp = visit_when_clauses(when_clauses, std::index_sequence_for<WhenClauses...>{});
            derived().visit_case_end();
            derived().visit_alias_impl(alias);
            return wp;
        }

        template <typename ElseExpr, typename... WhenClauses>
        constexpr auto visit(CaseExprWithElse<ElseExpr, WhenClauses...>&& expr) {
            auto&& [when_clauses, else_clause, alias] = std::move(expr).decompose();
            derived().visit_case_start();
            auto wp = visit_when_clauses(std::move(when_clauses), std::index_sequence_for<WhenClauses...>{});
            derived().visit_else_start();
            auto ep = std::move(else_clause).accept(derived());
            derived().visit_else_end();
            derived().visit_case_end();
            derived().visit_alias_impl(alias);
            return derived().cat_params(std::move(wp), std::move(ep));
        }

        template <typename ElseExpr, typename... WhenClauses>
        constexpr auto visit(const CaseExprWithElse<ElseExpr, WhenClauses...>& expr) {
            auto&& [when_clauses, else_clause, alias] = expr.decompose();
            derived().visit_case_start();
            auto wp = visit_when_clauses(when_clauses, std::index_sequence_for<WhenClauses...>{});
            derived().visit_else_start();
            auto ep = else_clause.accept(derived());
            derived().visit_else_end();
            derived().visit_case_end();
            derived().visit_alias_impl(alias);
            return derived().cat_params(std::move(wp), std::move(ep));
        }

        // CTE expressions - perfect forwarding
        template <typename Query>
        constexpr auto visit(CteExpr<Query>&& expr) {
            auto&& [name, query, recursive] = std::move(expr).decompose();
            derived().visit_cte_start(recursive);
            derived().visit_cte_name_impl(name);
            derived().visit_cte_as_start();
            auto qp = std::move(query).accept(derived());
            derived().visit_cte_as_end();
            derived().visit_cte_end();
            return qp;
        }

        template <typename Query>
        constexpr auto visit(const CteExpr<Query>& expr) {
            auto&& [name, query, recursive] = expr.decompose();
            derived().visit_cte_start(recursive);
            derived().visit_cte_name_impl(name);
            derived().visit_cte_as_start();
            auto qp = query.accept(derived());
            derived().visit_cte_as_end();
            derived().visit_cte_end();
            return qp;
        }

        template <IsTable TableT>
        constexpr auto visit(CreateTableExpr<TableT>&& expr) {
            auto&& [table, if_not_exists] = std::move(expr).decompose();
            derived().visit_create_table_start(if_not_exists);
            derived().visit_table_impl(table);
            derived().visit_create_table_columns(table);
            derived().visit_create_table_end();
            return derived().no_params();
        }

        template <IsTable TableT>
        constexpr auto visit(const CreateTableExpr<TableT>& expr) {
            auto&& [table, if_not_exists] = expr.decompose();
            derived().visit_create_table_start(if_not_exists);
            derived().visit_table_impl(table);
            derived().visit_create_table_columns(table);
            derived().visit_create_table_end();
            return derived().no_params();
        }

        template <IsTable TableT>
        constexpr auto visit(DropTableExpr<TableT>&& expr) {
            auto&& [table, if_exists, cascade] = std::move(expr).decompose();
            derived().visit_drop_table_start(if_exists);
            derived().visit_table_impl(table);
            derived().visit_drop_table_end(cascade);
            return derived().no_params();
        }

        template <IsTable TableT>
        constexpr auto visit(const DropTableExpr<TableT>& expr) {
            auto&& [table, if_exists, cascade] = expr.decompose();
            derived().visit_drop_table_start(if_exists);
            derived().visit_table_impl(table);
            derived().visit_drop_table_end(cascade);
            return derived().no_params();
        }

    private:
        // Helper to visit tuple elements with recursive approach
        // Base case: empty index sequence
        template <typename Tuple>
        constexpr auto visit_tuple_elements(Tuple&&, std::index_sequence<>) {
            return derived().no_params();
        }

        // Recursive case: visit first element by forward_like, recurse rest
        template <typename Tuple, std::size_t First, std::size_t... Rest>
        constexpr auto visit_tuple_elements(Tuple&& t, std::index_sequence<First, Rest...>) {
            auto head = visit_tuple_element(std::forward_like<Tuple>(std::get<First>(t)), First == 0);
            auto tail = visit_tuple_elements(std::forward<Tuple>(t), std::index_sequence<Rest...>{});
            return derived().cat_params(std::move(head), std::move(tail));
        }

        template <typename T>
        constexpr auto visit_tuple_element(T&& elem, const bool is_first) {
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

        // Recursive case: visit first when-clause by forward_like, recurse rest
        template <typename Tuple, std::size_t First, std::size_t... Rest>
        constexpr auto visit_when_clauses(Tuple&& t, std::index_sequence<First, Rest...>) {
            auto head = visit_single_when(std::forward_like<Tuple>(std::get<First>(t)));
            auto tail = visit_when_clauses(std::forward<Tuple>(t), std::index_sequence<Rest...>{});
            return derived().cat_params(std::move(head), std::move(tail));
        }

        template <typename WhenClause>
        constexpr auto visit_single_when(WhenClause&& when) {
            auto&& [cond, val] = std::forward<WhenClause>(when).decompose();
            derived().visit_when_start();
            auto cp = std::forward<decltype(cond)>(cond).accept(derived());
            derived().visit_when_then();
            auto vp = std::forward<decltype(val)>(val).accept(derived());
            derived().visit_when_end();
            return derived().cat_params(std::move(cp), std::move(vp));
        }
    };
}  // namespace demiplane::db

#include "detail/query_visitor.inl"
