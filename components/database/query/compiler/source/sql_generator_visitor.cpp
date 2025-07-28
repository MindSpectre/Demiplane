#include "sql_generator_visitor.hpp"

namespace demiplane::db {
    SqlGeneratorVisitor::SqlGeneratorVisitor(std::shared_ptr<SqlDialect> dialect, const bool use_params)
        : dialect_(std::move(dialect)),
          use_parameters_(use_params) {}

    void SqlGeneratorVisitor::visit_column_impl(const FieldSchema* schema, const char* table, const char* alias) {
        if (table) {
            sql_ << dialect_->quote_identifier(table) << ".";
        }
        sql_ << dialect_->quote_identifier(schema->name);
        if (alias && alias != table) {
            sql_ << " AS " << dialect_->quote_identifier(alias);
        }
    }

    void SqlGeneratorVisitor::visit_literal_impl(const FieldValue& value) {
        if (use_parameters_) {
            parameters_.push_back(value);
            sql_ << dialect_->placeholder(parameters_.size() - 1);
        }
        else {
            sql_ << dialect_->format_value(value);
        }
    }

    void SqlGeneratorVisitor::visit_null_impl() {
        sql_ << "NULL";
    }

    void SqlGeneratorVisitor::visit_all_columns_impl(const char* table) {
        if (table) {
            sql_ << dialect_->quote_identifier(table) << ".";
        }
        sql_ << "*";
    }

    void SqlGeneratorVisitor::visit_parameter_impl(const std::size_t index, std::type_index type) {
        sql_ << dialect_->placeholder(index);
    }

    void SqlGeneratorVisitor::visit_table_impl(const TableSchemaPtr& table) {
        sql_ << dialect_->quote_identifier(table->table_name());
    }

    void SqlGeneratorVisitor::visit_alias_impl(const char* alias) {
        sql_ << " AS " << dialect_->quote_identifier(alias);
    }

    void SqlGeneratorVisitor::visit_table_alias_impl(const char* alias) {
        sql_ << " AS " << dialect_->quote_identifier(alias);
    }

    void SqlGeneratorVisitor::visit_binary_expr_start() {
        sql_ << "(";
    }

    void SqlGeneratorVisitor::visit_binary_expr_end() {
        sql_ << ")";
    }

    void SqlGeneratorVisitor::visit_subquery_start() {
        sql_ << "(";
    }

    void SqlGeneratorVisitor::visit_subquery_end() {
        sql_ << ")";
    }

    void SqlGeneratorVisitor::visit_exists_start() {
        sql_ << "EXISTS (";
    }

    void SqlGeneratorVisitor::visit_exists_end() {
        sql_ << ")";
    }

    void SqlGeneratorVisitor::visit_binary_op_impl(OpEqual) {
        sql_ << " = ";
    }

    void SqlGeneratorVisitor::visit_binary_op_impl(OpNotEqual) {
        sql_ << " != ";
    }

    void SqlGeneratorVisitor::visit_binary_op_impl(OpLess) {
        sql_ << " < ";
    }

    void SqlGeneratorVisitor::visit_binary_op_impl(OpLessEqual) {
        sql_ << " <= ";
    }

    void SqlGeneratorVisitor::visit_binary_op_impl(OpGreater) {
        sql_ << " > ";
    }

    void SqlGeneratorVisitor::visit_binary_op_impl(OpGreaterEqual) {
        sql_ << " >= ";
    }

    void SqlGeneratorVisitor::visit_binary_op_impl(OpAnd) {
        sql_ << " AND ";
    }

    void SqlGeneratorVisitor::visit_binary_op_impl(OpOr) {
        sql_ << " OR ";
    }

    void SqlGeneratorVisitor::visit_binary_op_impl(OpLike) {
        sql_ << " LIKE ";
    }

    void SqlGeneratorVisitor::visit_binary_op_impl(OpNotLike) {
        sql_ << " NOT LIKE ";
    }

    void SqlGeneratorVisitor::visit_unary_op_impl(OpNot) {
        sql_ << "NOT ";
    }

    void SqlGeneratorVisitor::visit_unary_op_impl(OpIsNull) {
        sql_ << " IS NULL";
    }

    void SqlGeneratorVisitor::visit_unary_op_impl(OpIsNotNull) {
        sql_ << " IS NOT NULL";
    }

    void SqlGeneratorVisitor::visit_between_impl() {
        sql_ << " BETWEEN ";
    }

    void SqlGeneratorVisitor::visit_and_impl() {
        sql_ << " AND ";
    }

    void SqlGeneratorVisitor::visit_in_list_start() {
        sql_ << " IN (";
    }

    void SqlGeneratorVisitor::visit_in_list_end() {
        sql_ << ")";
    }

    void SqlGeneratorVisitor::visit_in_list_separator() {
        sql_ << ", ";
    }

    void SqlGeneratorVisitor::visit_count_impl(const bool distinct, const char* alias) {
        sql_ << "COUNT(";
        if (distinct) sql_ << "DISTINCT ";
    }

    void SqlGeneratorVisitor::visit_sum_impl(const char* alias) {
        sql_ << "SUM(";
    }

    void SqlGeneratorVisitor::visit_avg_impl(const char* alias) {
        sql_ << "AVG(";
    }

    void SqlGeneratorVisitor::visit_max_impl(const char* alias) {
        sql_ << "MAX(";
    }

    void SqlGeneratorVisitor::visit_min_impl(const char* alias) {
        sql_ << "MIN(";
    }

    void SqlGeneratorVisitor::visit_aggregate_end() {
        sql_ << ")";
    }

    void SqlGeneratorVisitor::visit_select_start(const bool distinct) {
        sql_ << "SELECT ";
        if (distinct) sql_ << "DISTINCT ";
    }

    void SqlGeneratorVisitor::visit_select_end() {}

    void SqlGeneratorVisitor::visit_from_start() {
        sql_ << " FROM ";
    }

    void SqlGeneratorVisitor::visit_from_end() {}

    void SqlGeneratorVisitor::visit_where_start() {
        sql_ << " WHERE ";
    }

    void SqlGeneratorVisitor::visit_where_end() {}

    void SqlGeneratorVisitor::visit_group_by_start() {
        sql_ << " GROUP BY ";
    }

    void SqlGeneratorVisitor::visit_group_by_end() {}

    void SqlGeneratorVisitor::visit_having_start() {
        sql_ << " HAVING ";
    }

    void SqlGeneratorVisitor::visit_having_end() {}

    void SqlGeneratorVisitor::visit_order_by_start() {
        sql_ << " ORDER BY ";
    }

    void SqlGeneratorVisitor::visit_order_by_end() {}

    void SqlGeneratorVisitor::visit_order_direction_impl(const OrderDirection dir) {
        switch (dir) {
            case OrderDirection::ASC:
                sql_ << " ASC";
                break;
            case OrderDirection::DESC:
                sql_ << " DESC";
                break;
        }
    }

    void SqlGeneratorVisitor::visit_limit_impl(const std::size_t limit, const std::size_t offset) {
        sql_ << dialect_->limit_clause(limit, offset);
    }

    void SqlGeneratorVisitor::visit_join_start(const JoinType type) {
        switch (type) {
            case JoinType::INNER:
                sql_ << " INNER JOIN ";
                break;
            case JoinType::LEFT:
                sql_ << " LEFT JOIN ";
                break;
            case JoinType::RIGHT:
                sql_ << " RIGHT JOIN ";
                break;
            case JoinType::FULL:
                sql_ << " FULL OUTER JOIN ";
                break;
            case JoinType::CROSS:
                sql_ << " CROSS JOIN ";
                break;
        }
    }

    void SqlGeneratorVisitor::visit_join_on() {
        sql_ << " ON ";
    }

    void SqlGeneratorVisitor::visit_join_end() {}

    void SqlGeneratorVisitor::visit_insert_start() {
        sql_ << "INSERT INTO ";
    }

    void SqlGeneratorVisitor::visit_insert_columns(const std::vector<std::string>& columns) {
        sql_ << " (";
        bool first = true;
        for (const auto& col : columns) {
            if (!first) sql_ << ", ";
            first = false;
            sql_ << dialect_->quote_identifier(col);
        }
        sql_ << ") VALUES ";
    }

    void SqlGeneratorVisitor::visit_insert_values(const std::vector<std::vector<FieldValue>>& rows) {
        bool first_row = true;
        for (const auto& row : rows) {
            if (!first_row) sql_ << ", ";
            first_row = false;
            sql_ << "(";
            bool first_val = true;
            for (const auto& val : row) {
                if (!first_val) sql_ << ", ";
                first_val = false;
                visit_literal_impl(val);
            }
            sql_ << ")";
        }
    }

    void SqlGeneratorVisitor::visit_insert_end() {}

    void SqlGeneratorVisitor::visit_update_start() {
        sql_ << "UPDATE ";
    }

    void SqlGeneratorVisitor::visit_update_set(const std::vector<std::pair<std::string, FieldValue>>& assignments) {
        sql_ << " SET ";
        bool first = true;
        for (const auto& [col, val] : assignments) {
            if (!first) sql_ << ", ";
            first = false;
            sql_ << dialect_->quote_identifier(col) << " = ";
            visit_literal_impl(val);
        }
    }

    void SqlGeneratorVisitor::visit_update_end() {}

    void SqlGeneratorVisitor::visit_delete_start() {
        sql_ << "DELETE FROM ";
    }

    void SqlGeneratorVisitor::visit_delete_end() {}

    void SqlGeneratorVisitor::visit_set_op_impl(const SetOperation op) {
        switch (op) {
            case SetOperation::UNION:
                sql_ << " UNION ";
                break;
            case SetOperation::UNION_ALL:
                sql_ << " UNION ALL ";
                break;
            case SetOperation::INTERSECT:
                sql_ << " INTERSECT ";
                break;
            case SetOperation::EXCEPT:
                sql_ << " EXCEPT ";
                break;
        }
    }

    void SqlGeneratorVisitor::visit_column_separator() {
        sql_ << ", ";
    }
}
