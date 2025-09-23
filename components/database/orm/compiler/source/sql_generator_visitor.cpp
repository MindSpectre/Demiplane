#include "sql_generator_visitor.hpp"

#include <iostream>


namespace demiplane::db {

    SqlGeneratorVisitor::SqlGeneratorVisitor(std::shared_ptr<SqlDialect> d,
                                             std::pmr::memory_resource* mr,
                                             const bool use_params)
        : dialect_(std::move(d)),
          sql_{mr},
          use_params_{use_params} {
        if (use_params_) {
            packet_ = dialect_->make_param_sink(mr);
            sink_   = packet_.sink.get();
        }
    }
    void SqlGeneratorVisitor::visit_table_column_impl(const FieldSchema* schema,
                                                      const std::shared_ptr<std::string>& table,
                                                      const std::optional<std::string>& alias) {
        if (table) {
            visit_table_impl(table);
            sql_ += ".";
        }
        dialect_->quote_identifier(sql_, schema->name);
        visit_alias_impl(alias);
    }

    void SqlGeneratorVisitor::visit_dynamic_column_impl(const std::string& name,
                                                        const std::optional<std::string>& table) {
        if (table) {
            visit_table_impl(table.value());
            sql_ += ".";
        }
        dialect_->quote_identifier(sql_, name);
    }

    void SqlGeneratorVisitor::visit_value_impl(const FieldValue& v)  {
        if (use_params_) {
            const std::size_t idx = sink_->push(v);
            dialect_->placeholder(sql_, idx);
        } else {
            dialect_->format_value(sql_, v);
        }
    }

    void SqlGeneratorVisitor::visit_value_impl(FieldValue&& v) {
        if (use_params_) {
            const std::size_t idx = sink_->push(std::move(v));
            dialect_->placeholder(sql_, idx);
        } else {
            dialect_->format_value(sql_, v);
        }
    }

    void SqlGeneratorVisitor::visit_null_impl() {
        sql_ += "NULL";
    }

    void SqlGeneratorVisitor::visit_all_columns_impl(const std::shared_ptr<std::string>& table) {
        if (table) {
            visit_table_impl(*table);
            sql_ += ".";
        }
        sql_ += "*";
    }


    void SqlGeneratorVisitor::visit_table_impl(const TablePtr& table) {
        dialect_->quote_identifier(sql_, table->table_name());
    }


    void SqlGeneratorVisitor::visit_table_impl(const std::string_view table_name) {
        dialect_->quote_identifier(sql_, table_name);
    }

    void SqlGeneratorVisitor::visit_table_impl(const std::shared_ptr<std::string>& table) {
        if (table) {
            dialect_->quote_identifier(sql_, *table);
        }
    }

    void SqlGeneratorVisitor::visit_alias_impl(const std::string_view alias) {
        sql_ += " AS ";
        dialect_->quote_identifier(sql_, alias);
    }

    void SqlGeneratorVisitor::visit_alias_impl(const std::optional<std::string>& alias) {
        if (alias.has_value()) {
            sql_ += " AS ";
            dialect_->quote_identifier(sql_, alias.value());
        }
    }


    void SqlGeneratorVisitor::visit_binary_expr_start() {
        sql_ += "(";
    }

    void SqlGeneratorVisitor::visit_binary_expr_end() {
        sql_ += ")";
    }

    void SqlGeneratorVisitor::visit_subquery_start() {
        sql_ += "(";
    }

    void SqlGeneratorVisitor::visit_subquery_end() {
        sql_ += ")";
    }

    void SqlGeneratorVisitor::visit_exists_start() {
        sql_ += "EXISTS (";
    }

    void SqlGeneratorVisitor::visit_exists_end() {
        sql_ += ")";
    }

    void SqlGeneratorVisitor::visit_binary_op_impl(OpEqual) {
        sql_ += " = ";
    }

    void SqlGeneratorVisitor::visit_binary_op_impl(OpNotEqual) {
        sql_ += " != ";
    }

    void SqlGeneratorVisitor::visit_binary_op_impl(OpLess) {
        sql_ += " < ";
    }

    void SqlGeneratorVisitor::visit_binary_op_impl(OpLessEqual) {
        sql_ += " <= ";
    }

    void SqlGeneratorVisitor::visit_binary_op_impl(OpGreater) {
        sql_ += " > ";
    }

    void SqlGeneratorVisitor::visit_binary_op_impl(OpGreaterEqual) {
        sql_ += " >= ";
    }

    void SqlGeneratorVisitor::visit_binary_op_impl(OpAnd) {
        sql_ += " AND ";
    }

    void SqlGeneratorVisitor::visit_binary_op_impl(OpOr) {
        sql_ += " OR ";
    }

    void SqlGeneratorVisitor::visit_binary_op_impl(OpLike) {
        sql_ += " LIKE ";
    }

    void SqlGeneratorVisitor::visit_binary_op_impl(OpNotLike) {
        sql_ += " NOT LIKE ";
    }

    void SqlGeneratorVisitor::visit_binary_op_impl(OpIn) {
        sql_ += " IN ";
    }

    void SqlGeneratorVisitor::visit_unary_op_impl(OpNot) {
        sql_ += "NOT ";
    }

    void SqlGeneratorVisitor::visit_unary_op_impl(OpIsNull) {
        sql_ += " IS NULL";
    }

    void SqlGeneratorVisitor::visit_unary_op_impl(OpIsNotNull) {
        sql_ += " IS NOT NULL";
    }

    void SqlGeneratorVisitor::visit_between_impl() {
        sql_ += " BETWEEN ";
    }

    void SqlGeneratorVisitor::visit_and_impl() {
        sql_ += " AND ";
    }

    void SqlGeneratorVisitor::visit_in_list_start() {
        sql_ += " IN (";
    }

    void SqlGeneratorVisitor::visit_in_list_end() {
        sql_ += ")";
    }

    void SqlGeneratorVisitor::visit_in_list_separator() {
        sql_ += ", ";
    }

    void SqlGeneratorVisitor::visit_count_impl(const bool distinct) {
        sql_ += "COUNT(";
        if (distinct)
            sql_ += "DISTINCT ";
    }

    void SqlGeneratorVisitor::visit_sum_impl() {
        sql_ += "SUM(";
    }

    void SqlGeneratorVisitor::visit_avg_impl() {
        sql_ += "AVG(";
    }

    void SqlGeneratorVisitor::visit_max_impl() {
        sql_ += "MAX(";
    }

    void SqlGeneratorVisitor::visit_min_impl() {
        sql_ += "MIN(";
    }

    void SqlGeneratorVisitor::visit_aggregate_end(const std::optional<std::string>& alias) {
        sql_ += ")";
        visit_alias_impl(alias);
    }


    void SqlGeneratorVisitor::visit_select_start(const bool distinct) {
        sql_ += "SELECT ";
        if (distinct)
            sql_ += "DISTINCT ";
    }

    void SqlGeneratorVisitor::visit_select_end() {
    }

    void SqlGeneratorVisitor::visit_from_start() {
        sql_ += " FROM ";
    }

    void SqlGeneratorVisitor::visit_from_end() {
    }

    void SqlGeneratorVisitor::visit_where_start() {
        sql_ += " WHERE ";
    }

    void SqlGeneratorVisitor::visit_where_end() {
    }

    void SqlGeneratorVisitor::visit_group_by_start() {
        sql_ += " GROUP BY ";
    }

    void SqlGeneratorVisitor::visit_group_by_end() {
    }

    void SqlGeneratorVisitor::visit_having_start() {
        sql_ += " HAVING ";
    }

    void SqlGeneratorVisitor::visit_having_end() {
    }

    void SqlGeneratorVisitor::visit_order_by_start() {
        sql_ += " ORDER BY ";
    }

    void SqlGeneratorVisitor::visit_order_by_end() {
    }

    void SqlGeneratorVisitor::visit_order_direction_impl(const OrderDirection dir) {
        switch (dir) {
            case OrderDirection::ASC:
                sql_ += " ASC";
                break;
            case OrderDirection::DESC:
                sql_ += " DESC";
                break;
        }
    }

    void SqlGeneratorVisitor::visit_limit_impl(const std::size_t limit, const std::size_t offset) {
        sql_ += dialect_->limit_clause(limit, offset);
    }

    void SqlGeneratorVisitor::visit_join_start(const JoinType type) {
        switch (type) {
            case JoinType::INNER:
                sql_ += " INNER JOIN ";
                break;
            case JoinType::LEFT:
                sql_ += " LEFT JOIN ";
                break;
            case JoinType::RIGHT:
                sql_ += " RIGHT JOIN ";
                break;
            case JoinType::FULL:
                sql_ += " FULL OUTER JOIN ";
                break;
            case JoinType::CROSS:
                sql_ += " CROSS JOIN ";
                break;
        }
    }

    void SqlGeneratorVisitor::visit_join_on() {
        sql_ += " ON ";
    }

    void SqlGeneratorVisitor::visit_join_end() {
    }

    void SqlGeneratorVisitor::visit_insert_start() {
        sql_ += "INSERT INTO ";
    }

    void SqlGeneratorVisitor::visit_insert_columns(const std::vector<std::string>& columns) {
        sql_       += " (";
        bool first = true;
        for (const auto& col : columns) {
            if (!first)
                sql_ += ", ";
            first = false;
            dialect_->quote_identifier(sql_, col);
        }
        sql_ += ") VALUES ";
    }

    void SqlGeneratorVisitor::visit_insert_columns(std::vector<std::string>&& columns) {
        sql_       += " (";
        bool first = true;
        for (const auto& col : columns) {
            if (!first)
                sql_ += ", ";
            first = false;
            dialect_->quote_identifier(sql_, col);
        }
        sql_ += ") VALUES ";
    }

    void SqlGeneratorVisitor::visit_insert_values(const std::vector<std::vector<FieldValue>>& rows) {
        bool first_row = true;
        for (const auto& row : rows) {
            if (!first_row)
                sql_ += ", ";
            first_row       = false;
            sql_           += "(";
            bool first_val = true;
            for (const auto& val : row) {
                if (!first_val)
                    sql_ += ", ";
                first_val = false;
                visit_value_impl(val);
            }
            sql_ += ")";
        }
    }

    void SqlGeneratorVisitor::visit_insert_values(std::vector<std::vector<FieldValue>>&& rows) {
        bool first_row = true;
        for (auto&& row : std::move(rows)) {
            if (!first_row)
                sql_ += ", ";
            first_row       = false;
            sql_ += "(";
            bool first_val = true;
            for (auto&& val : std::move(row)) {
                if (!first_val)
                    sql_ += ", ";
                first_val = false;
                visit_value_impl(std::move(val));
            }
            sql_ += ")";
        }
    }

    void SqlGeneratorVisitor::visit_insert_end() {
    }

    void SqlGeneratorVisitor::visit_update_start() {
        sql_ += "UPDATE ";
    }

    void SqlGeneratorVisitor::visit_update_set(const std::vector<std::pair<std::string, FieldValue>>& assignments) {
        sql_ += " SET ";
        bool first = true;
        for (const auto& [col, val] : assignments) {
            if (!first)
                sql_ += ", ";
            first = false;
            dialect_->quote_identifier(sql_, col);
            sql_ += " = ";
            visit_value_impl(val);
        }
    }

    void SqlGeneratorVisitor::visit_update_set(std::vector<std::pair<std::string, FieldValue>>&& assignments) {
        sql_ += " SET ";
        bool first = true;
        for (auto&& [col, val] : std::move(assignments)) {
            if (!first)
                sql_ += ", ";
            first = false;
            dialect_->quote_identifier(sql_, col);
            sql_ += " = ";
            visit_value_impl(std::move(val));
        }
    }

    void SqlGeneratorVisitor::visit_update_end() {
    }

    void SqlGeneratorVisitor::visit_delete_start() {
        sql_ += "DELETE FROM ";
    }

    void SqlGeneratorVisitor::visit_delete_end() {
    }

    void SqlGeneratorVisitor::visit_case_start() {
        sql_ += "CASE";
    }

    void SqlGeneratorVisitor::visit_case_end() {
        sql_ += " END";
    }

    void SqlGeneratorVisitor::visit_when_start() {
        sql_ += " WHEN ";
    }

    void SqlGeneratorVisitor::visit_when_then() {
        sql_ += " THEN ";
    }

    void SqlGeneratorVisitor::visit_when_end() {
        // Nothing needed after each WHEN clause
    }

    void SqlGeneratorVisitor::visit_else_start() {
        sql_ += " ELSE ";
    }

    void SqlGeneratorVisitor::visit_else_end() {
        // Nothing needed after ELSE clause
    }

    void SqlGeneratorVisitor::visit_set_op_impl(const SetOperation op) {
        switch (op) {
            case SetOperation::UNION:
                sql_ += " UNION ";
                break;
            case SetOperation::UNION_ALL:
                sql_ += " UNION ALL ";
                break;
            case SetOperation::INTERSECT:
                sql_ += " INTERSECT ";
                break;
            case SetOperation::EXCEPT:
                sql_ += " EXCEPT ";
                break;
        }
    }

    void SqlGeneratorVisitor::visit_cte_start(const bool recursive) {
        sql_ += "WITH ";
        if (recursive) {
            sql_ += "RECURSIVE ";
        }
    }

    void SqlGeneratorVisitor::visit_cte_name_impl(const std::string_view name) {
        dialect_->quote_identifier(sql_, name);
    }

    void SqlGeneratorVisitor::visit_cte_as_start() {
        sql_ += " AS (";
    }

    void SqlGeneratorVisitor::visit_cte_as_end() {
        sql_ += ")";
    }

    void SqlGeneratorVisitor::visit_cte_end() {
        sql_ += " ";
    }

    void SqlGeneratorVisitor::visit_column_separator() {
        sql_ += ", ";
    }
}  // namespace demiplane::db
