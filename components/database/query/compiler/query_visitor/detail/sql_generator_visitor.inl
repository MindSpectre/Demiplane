#pragma once
#include <expected>
namespace demiplane::db {

    template <IsSqlDialect DialectT, Appendable StringT>
    constexpr void SqlGeneratorVisitor<DialectT, StringT>::visit_table_column_impl(const FieldSchema* schema,
                                                                                   const std::string_view table,
                                                                                   const std::string_view alias) {
        if (!table.empty()) {
            visit_table_impl(table);
            sql_ += ".";
        }
        DialectT::quote_identifier(sql_, schema->name);
        visit_alias_impl(alias);
    }

    template <IsSqlDialect DialectT, Appendable StringT>
    constexpr void SqlGeneratorVisitor<DialectT, StringT>::visit_dynamic_column_impl(const std::string_view name,
                                                                                     const std::string_view table) {
        if (!table.empty()) {
            visit_table_impl(table);
            sql_ += ".";
        }
        DialectT::quote_identifier(sql_, name);
    }

    template <IsSqlDialect DialectT, Appendable StringT>
    constexpr void SqlGeneratorVisitor<DialectT, StringT>::visit_value_impl(const FieldValue& value) {
        if consteval {
            DialectT::format_value(sql_, value);
        } else {
            if (use_params_ && sink_) {
                const std::size_t idx = sink_->push(value);
                DialectT::placeholder(sql_, idx);
            } else {
                DialectT::format_value(sql_, value);
            }
        }
    }

    template <IsSqlDialect DialectT, Appendable StringT>
    constexpr void SqlGeneratorVisitor<DialectT, StringT>::visit_value_impl(FieldValue&& value) {
        if consteval {
            DialectT::format_value(sql_, value);
        } else {
            if (use_params_ && sink_) {
                const std::size_t idx = sink_->push(std::move(value));
                DialectT::placeholder(sql_, idx);
            } else {
                DialectT::format_value(sql_, value);
            }
        }
    }

    template <IsSqlDialect DialectT, Appendable StringT>
    constexpr void SqlGeneratorVisitor<DialectT, StringT>::visit_null_impl() {
        sql_ += "NULL";
    }

    template <IsSqlDialect DialectT, Appendable StringT>
    constexpr void SqlGeneratorVisitor<DialectT, StringT>::visit_all_columns_impl(const std::string_view table) {
        if (!table.empty()) {
            visit_table_impl(table);
            sql_ += ".";
        }
        sql_ += "*";
    }

    template <IsSqlDialect DialectT, Appendable StringT>
    constexpr void SqlGeneratorVisitor<DialectT, StringT>::visit_table_impl(const TablePtr& table) {
        if (!table)
            return;
        DialectT::quote_identifier(sql_, table->table_name());
    }

    template <IsSqlDialect DialectT, Appendable StringT>
    constexpr void SqlGeneratorVisitor<DialectT, StringT>::visit_table_impl(const std::string_view table_name) {
        if (table_name.empty())
            return;
        DialectT::quote_identifier(sql_, table_name);
    }

    template <IsSqlDialect DialectT, Appendable StringT>
    constexpr void SqlGeneratorVisitor<DialectT, StringT>::visit_alias_impl(const std::string_view alias) {
        if (alias.empty())
            return;
        sql_ += " AS ";
        DialectT::quote_identifier(sql_, alias);
    }

    template <IsSqlDialect DialectT, Appendable StringT>
    constexpr void SqlGeneratorVisitor<DialectT, StringT>::visit_binary_expr_start() {
        sql_ += "(";
    }

    template <IsSqlDialect DialectT, Appendable StringT>
    constexpr void SqlGeneratorVisitor<DialectT, StringT>::visit_binary_expr_end() {
        sql_ += ")";
    }

    template <IsSqlDialect DialectT, Appendable StringT>
    constexpr void SqlGeneratorVisitor<DialectT, StringT>::visit_subquery_start() {
        sql_ += "(";
    }

    template <IsSqlDialect DialectT, Appendable StringT>
    constexpr void SqlGeneratorVisitor<DialectT, StringT>::visit_subquery_end() {
        sql_ += ")";
    }

    template <IsSqlDialect DialectT, Appendable StringT>
    constexpr void SqlGeneratorVisitor<DialectT, StringT>::visit_exists_start() {
        sql_ += "EXISTS (";
    }

    template <IsSqlDialect DialectT, Appendable StringT>
    constexpr void SqlGeneratorVisitor<DialectT, StringT>::visit_exists_end() {
        sql_ += ")";
    }

    template <IsSqlDialect DialectT, Appendable StringT>
    constexpr void SqlGeneratorVisitor<DialectT, StringT>::visit_binary_op_impl(OpEqual) {
        sql_ += " = ";
    }

    template <IsSqlDialect DialectT, Appendable StringT>
    constexpr void SqlGeneratorVisitor<DialectT, StringT>::visit_binary_op_impl(OpNotEqual) {
        sql_ += " != ";
    }

    template <IsSqlDialect DialectT, Appendable StringT>
    constexpr void SqlGeneratorVisitor<DialectT, StringT>::visit_binary_op_impl(OpLess) {
        sql_ += " < ";
    }

    template <IsSqlDialect DialectT, Appendable StringT>
    constexpr void SqlGeneratorVisitor<DialectT, StringT>::visit_binary_op_impl(OpLessEqual) {
        sql_ += " <= ";
    }

    template <IsSqlDialect DialectT, Appendable StringT>
    constexpr void SqlGeneratorVisitor<DialectT, StringT>::visit_binary_op_impl(OpGreater) {
        sql_ += " > ";
    }

    template <IsSqlDialect DialectT, Appendable StringT>
    constexpr void SqlGeneratorVisitor<DialectT, StringT>::visit_binary_op_impl(OpGreaterEqual) {
        sql_ += " >= ";
    }

    template <IsSqlDialect DialectT, Appendable StringT>
    constexpr void SqlGeneratorVisitor<DialectT, StringT>::visit_binary_op_impl(OpAnd) {
        sql_ += " AND ";
    }

    template <IsSqlDialect DialectT, Appendable StringT>
    constexpr void SqlGeneratorVisitor<DialectT, StringT>::visit_binary_op_impl(OpOr) {
        sql_ += " OR ";
    }

    template <IsSqlDialect DialectT, Appendable StringT>
    constexpr void SqlGeneratorVisitor<DialectT, StringT>::visit_binary_op_impl(OpLike) {
        sql_ += " LIKE ";
    }

    template <IsSqlDialect DialectT, Appendable StringT>
    constexpr void SqlGeneratorVisitor<DialectT, StringT>::visit_binary_op_impl(OpNotLike) {
        sql_ += " NOT LIKE ";
    }

    template <IsSqlDialect DialectT, Appendable StringT>
    constexpr void SqlGeneratorVisitor<DialectT, StringT>::visit_binary_op_impl(OpIn) {
        sql_ += " IN ";
    }

    template <IsSqlDialect DialectT, Appendable StringT>
    constexpr void SqlGeneratorVisitor<DialectT, StringT>::visit_unary_op_impl(OpNot) {
        sql_ += "NOT ";
    }

    template <IsSqlDialect DialectT, Appendable StringT>
    constexpr void SqlGeneratorVisitor<DialectT, StringT>::visit_unary_op_impl(OpIsNull) {
        sql_ += " IS NULL";
    }

    template <IsSqlDialect DialectT, Appendable StringT>
    constexpr void SqlGeneratorVisitor<DialectT, StringT>::visit_unary_op_impl(OpIsNotNull) {
        sql_ += " IS NOT NULL";
    }

    template <IsSqlDialect DialectT, Appendable StringT>
    constexpr void SqlGeneratorVisitor<DialectT, StringT>::visit_between_impl() {
        sql_ += " BETWEEN ";
    }

    template <IsSqlDialect DialectT, Appendable StringT>
    constexpr void SqlGeneratorVisitor<DialectT, StringT>::visit_and_impl() {
        sql_ += " AND ";
    }

    template <IsSqlDialect DialectT, Appendable StringT>
    constexpr void SqlGeneratorVisitor<DialectT, StringT>::visit_in_list_start() {
        sql_ += " IN (";
    }

    template <IsSqlDialect DialectT, Appendable StringT>
    constexpr void SqlGeneratorVisitor<DialectT, StringT>::visit_in_list_end() {
        sql_ += ")";
    }

    template <IsSqlDialect DialectT, Appendable StringT>
    constexpr void SqlGeneratorVisitor<DialectT, StringT>::visit_in_list_separator() {
        sql_ += ", ";
    }

    template <IsSqlDialect DialectT, Appendable StringT>
    constexpr void SqlGeneratorVisitor<DialectT, StringT>::visit_count_impl(const bool distinct) {
        sql_ += "COUNT(";
        if (distinct)
            sql_ += "DISTINCT ";
    }

    template <IsSqlDialect DialectT, Appendable StringT>
    constexpr void SqlGeneratorVisitor<DialectT, StringT>::visit_sum_impl() {
        sql_ += "SUM(";
    }

    template <IsSqlDialect DialectT, Appendable StringT>
    constexpr void SqlGeneratorVisitor<DialectT, StringT>::visit_avg_impl() {
        sql_ += "AVG(";
    }

    template <IsSqlDialect DialectT, Appendable StringT>
    constexpr void SqlGeneratorVisitor<DialectT, StringT>::visit_max_impl() {
        sql_ += "MAX(";
    }

    template <IsSqlDialect DialectT, Appendable StringT>
    constexpr void SqlGeneratorVisitor<DialectT, StringT>::visit_min_impl() {
        sql_ += "MIN(";
    }

    template <IsSqlDialect DialectT, Appendable StringT>
    constexpr void SqlGeneratorVisitor<DialectT, StringT>::visit_aggregate_end(const std::string_view alias) {
        sql_ += ")";
        visit_alias_impl(alias);
    }

    template <IsSqlDialect DialectT, Appendable StringT>
    constexpr void SqlGeneratorVisitor<DialectT, StringT>::visit_select_start(const bool distinct) {
        sql_ += "SELECT ";
        if (distinct)
            sql_ += "DISTINCT ";
    }

    template <IsSqlDialect DialectT, Appendable StringT>
    constexpr void SqlGeneratorVisitor<DialectT, StringT>::visit_select_end() {
    }

    template <IsSqlDialect DialectT, Appendable StringT>
    constexpr void SqlGeneratorVisitor<DialectT, StringT>::visit_from_start() {
        sql_ += " FROM ";
    }

    template <IsSqlDialect DialectT, Appendable StringT>
    constexpr void SqlGeneratorVisitor<DialectT, StringT>::visit_from_end() {
    }

    template <IsSqlDialect DialectT, Appendable StringT>
    constexpr void SqlGeneratorVisitor<DialectT, StringT>::visit_where_start() {
        sql_ += " WHERE ";
    }

    template <IsSqlDialect DialectT, Appendable StringT>
    constexpr void SqlGeneratorVisitor<DialectT, StringT>::visit_where_end() {
    }

    template <IsSqlDialect DialectT, Appendable StringT>
    constexpr void SqlGeneratorVisitor<DialectT, StringT>::visit_group_by_start() {
        sql_ += " GROUP BY ";
    }

    template <IsSqlDialect DialectT, Appendable StringT>
    constexpr void SqlGeneratorVisitor<DialectT, StringT>::visit_group_by_end() {
    }

    template <IsSqlDialect DialectT, Appendable StringT>
    constexpr void SqlGeneratorVisitor<DialectT, StringT>::visit_having_start() {
        sql_ += " HAVING ";
    }

    template <IsSqlDialect DialectT, Appendable StringT>
    constexpr void SqlGeneratorVisitor<DialectT, StringT>::visit_having_end() {
    }

    template <IsSqlDialect DialectT, Appendable StringT>
    constexpr void SqlGeneratorVisitor<DialectT, StringT>::visit_order_by_start() {
        sql_ += " ORDER BY ";
    }

    template <IsSqlDialect DialectT, Appendable StringT>
    constexpr void SqlGeneratorVisitor<DialectT, StringT>::visit_order_by_end() {
    }

    template <IsSqlDialect DialectT, Appendable StringT>
    constexpr void SqlGeneratorVisitor<DialectT, StringT>::visit_order_direction_impl(const OrderDirection dir) {
        switch (dir) {
            case OrderDirection::ASC:
                sql_ += " ASC";
                break;
            case OrderDirection::DESC:
                sql_ += " DESC";
                break;
        }
    }

    template <IsSqlDialect DialectT, Appendable StringT>
    constexpr void SqlGeneratorVisitor<DialectT, StringT>::visit_limit_impl(const std::size_t limit,
                                                                            const std::size_t offset) {
        DialectT::limit_clause(sql_, limit, offset);
    }

    template <IsSqlDialect DialectT, Appendable StringT>
    constexpr void SqlGeneratorVisitor<DialectT, StringT>::visit_join_start(const JoinType type) {
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

    template <IsSqlDialect DialectT, Appendable StringT>
    constexpr void SqlGeneratorVisitor<DialectT, StringT>::visit_join_on() {
        sql_ += " ON ";
    }

    template <IsSqlDialect DialectT, Appendable StringT>
    constexpr void SqlGeneratorVisitor<DialectT, StringT>::visit_join_end() {
    }

    template <IsSqlDialect DialectT, Appendable StringT>
    constexpr void SqlGeneratorVisitor<DialectT, StringT>::visit_insert_start() {
        sql_ += "INSERT INTO ";
    }

    template <IsSqlDialect DialectT, Appendable StringT>
    constexpr void
    SqlGeneratorVisitor<DialectT, StringT>::visit_insert_columns(const std::vector<std::string>& columns) {
        sql_       += " (";
        bool first  = true;
        for (const auto& col : columns) {
            if (!first)
                sql_ += ", ";
            first = false;
            DialectT::quote_identifier(sql_, col);
        }
        sql_ += ") VALUES ";
    }

    template <IsSqlDialect DialectT, Appendable StringT>
    constexpr void SqlGeneratorVisitor<DialectT, StringT>::visit_insert_columns(std::vector<std::string>&& columns) {
        sql_       += " (";
        bool first  = true;
        for (const auto& col : columns) {
            if (!first)
                sql_ += ", ";
            first = false;
            DialectT::quote_identifier(sql_, col);
        }
        sql_ += ") VALUES ";
    }

    template <IsSqlDialect DialectT, Appendable StringT>
    constexpr void
    SqlGeneratorVisitor<DialectT, StringT>::visit_insert_values(const std::vector<std::vector<FieldValue>>& rows) {
        bool first_row = true;
        for (const auto& row : rows) {
            if (!first_row)
                sql_ += ", ";
            first_row       = false;
            sql_           += "(";
            bool first_val  = true;
            for (const auto& val : row) {
                if (!first_val)
                    sql_ += ", ";
                first_val = false;
                visit_value_impl(val);
            }
            sql_ += ")";
        }
    }

    template <IsSqlDialect DialectT, Appendable StringT>
    constexpr void
    SqlGeneratorVisitor<DialectT, StringT>::visit_insert_values(std::vector<std::vector<FieldValue>>&& rows) {
        bool first_row = true;
        for (auto&& row : std::move(rows)) {
            if (!first_row)
                sql_ += ", ";
            first_row       = false;
            sql_           += "(";
            bool first_val  = true;
            for (auto&& val : std::move(row)) {
                if (!first_val)
                    sql_ += ", ";
                first_val = false;
                visit_value_impl(std::move(val));
            }
            sql_ += ")";
        }
    }

    template <IsSqlDialect DialectT, Appendable StringT>
    constexpr void SqlGeneratorVisitor<DialectT, StringT>::visit_insert_end() {
    }

    template <IsSqlDialect DialectT, Appendable StringT>
    constexpr void SqlGeneratorVisitor<DialectT, StringT>::visit_update_start() {
        sql_ += "UPDATE ";
    }

    template <IsSqlDialect DialectT, Appendable StringT>
    constexpr void SqlGeneratorVisitor<DialectT, StringT>::visit_update_set(
        const std::vector<std::pair<std::string, FieldValue>>& assignments) {
        sql_       += " SET ";
        bool first  = true;
        for (const auto& [col, val] : assignments) {
            if (!first)
                sql_ += ", ";
            first = false;
            DialectT::quote_identifier(sql_, col);
            sql_ += " = ";
            visit_value_impl(val);
        }
    }

    template <IsSqlDialect DialectT, Appendable StringT>
    constexpr void SqlGeneratorVisitor<DialectT, StringT>::visit_update_set(
        std::vector<std::pair<std::string, FieldValue>>&& assignments) {
        sql_       += " SET ";
        bool first  = true;
        for (auto&& [col, val] : std::move(assignments)) {
            if (!first)
                sql_ += ", ";
            first = false;
            DialectT::quote_identifier(sql_, col);
            sql_ += " = ";
            visit_value_impl(std::move(val));
        }
    }

    template <IsSqlDialect DialectT, Appendable StringT>
    constexpr void SqlGeneratorVisitor<DialectT, StringT>::visit_update_end() {
    }

    template <IsSqlDialect DialectT, Appendable StringT>
    constexpr void SqlGeneratorVisitor<DialectT, StringT>::visit_delete_start() {
        sql_ += "DELETE FROM ";
    }

    template <IsSqlDialect DialectT, Appendable StringT>
    constexpr void SqlGeneratorVisitor<DialectT, StringT>::visit_delete_end() {
    }

    template <IsSqlDialect DialectT, Appendable StringT>
    constexpr void SqlGeneratorVisitor<DialectT, StringT>::visit_case_start() {
        sql_ += "CASE";
    }

    template <IsSqlDialect DialectT, Appendable StringT>
    constexpr void SqlGeneratorVisitor<DialectT, StringT>::visit_case_end() {
        sql_ += " END";
    }

    template <IsSqlDialect DialectT, Appendable StringT>
    constexpr void SqlGeneratorVisitor<DialectT, StringT>::visit_when_start() {
        sql_ += " WHEN ";
    }

    template <IsSqlDialect DialectT, Appendable StringT>
    constexpr void SqlGeneratorVisitor<DialectT, StringT>::visit_when_then() {
        sql_ += " THEN ";
    }

    template <IsSqlDialect DialectT, Appendable StringT>
    constexpr void SqlGeneratorVisitor<DialectT, StringT>::visit_when_end() {
        // Nothing needed after each WHEN clause
    }

    template <IsSqlDialect DialectT, Appendable StringT>
    constexpr void SqlGeneratorVisitor<DialectT, StringT>::visit_else_start() {
        sql_ += " ELSE ";
    }

    template <IsSqlDialect DialectT, Appendable StringT>
    constexpr void SqlGeneratorVisitor<DialectT, StringT>::visit_else_end() {
        // Nothing needed after ELSE clause
    }

    template <IsSqlDialect DialectT, Appendable StringT>
    constexpr void SqlGeneratorVisitor<DialectT, StringT>::visit_set_op_impl(const SetOperation op) {
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

    template <IsSqlDialect DialectT, Appendable StringT>
    constexpr void SqlGeneratorVisitor<DialectT, StringT>::visit_cte_start(const bool recursive) {
        sql_ += "WITH ";
        if (recursive) {
            sql_ += "RECURSIVE ";
        }
    }

    template <IsSqlDialect DialectT, Appendable StringT>
    constexpr void SqlGeneratorVisitor<DialectT, StringT>::visit_cte_name_impl(const std::string_view name) {
        DialectT::quote_identifier(sql_, name);
    }

    template <IsSqlDialect DialectT, Appendable StringT>
    constexpr void SqlGeneratorVisitor<DialectT, StringT>::visit_cte_as_start() {
        sql_ += " AS (";
    }

    template <IsSqlDialect DialectT, Appendable StringT>
    constexpr void SqlGeneratorVisitor<DialectT, StringT>::visit_cte_as_end() {
        sql_ += ")";
    }

    template <IsSqlDialect DialectT, Appendable StringT>
    constexpr void SqlGeneratorVisitor<DialectT, StringT>::visit_cte_end() {
        sql_ += " ";
    }

    template <IsSqlDialect DialectT, Appendable StringT>
    constexpr void SqlGeneratorVisitor<DialectT, StringT>::visit_create_table_start(const bool if_not_exists) {
        sql_ += "CREATE TABLE ";
        if (if_not_exists) {
            sql_ += "IF NOT EXISTS ";
        }
    }

    template <IsSqlDialect DialectT, Appendable StringT>
    constexpr void SqlGeneratorVisitor<DialectT, StringT>::visit_create_table_columns(const TablePtr& table) {
        if (!table)
            return;

        sql_ += " (";

        bool first = true;
        for (const auto& field : table->fields()) {
            if (!first)
                sql_ += ", ";
            first = false;

            // Column name
            DialectT::quote_identifier(sql_, field->name);
            sql_ += " ";

            // Column type
            sql_ += field->db_type;

            // PRIMARY KEY constraint
            if (field->is_primary_key) {
                sql_ += " PRIMARY KEY";
            }

            // NOT NULL constraint (skip if primary key - already implies NOT NULL)
            if (!field->is_nullable && !field->is_primary_key) {
                sql_ += " NOT NULL";
            }

            // UNIQUE constraint (skip if primary key - already unique)
            if (field->is_unique && !field->is_primary_key) {
                sql_ += " UNIQUE";
            }

            // DEFAULT value
            if (!field->default_value.empty()) {
                sql_ += " DEFAULT ";
                sql_ += field->default_value;
            }

            // FOREIGN KEY constraint
            if (field->is_foreign_key && !field->foreign_table.empty()) {
                sql_ += " REFERENCES ";
                DialectT::quote_identifier(sql_, field->foreign_table);
                sql_ += "(";
                DialectT::quote_identifier(sql_, field->foreign_column);
                sql_ += ")";
            }
        }
        sql_ += ")";
    }

    template <IsSqlDialect DialectT, Appendable StringT>
    constexpr void SqlGeneratorVisitor<DialectT, StringT>::visit_create_table_end() {
        // PostgreSQL doesn't need anything here //TODO? sql visitor must be unified(call dialect)
        // Other dialects might add ENGINE, CHARSET, etc.
    }

    template <IsSqlDialect DialectT, Appendable StringT>
    constexpr void SqlGeneratorVisitor<DialectT, StringT>::visit_drop_table_start(const bool if_exists) {
        sql_ += "DROP TABLE ";
        if (if_exists) {
            sql_ += "IF EXISTS ";
        }
    }

    template <IsSqlDialect DialectT, Appendable StringT>
    constexpr void SqlGeneratorVisitor<DialectT, StringT>::visit_drop_table_end(const bool cascade) {
        if (cascade) {
            sql_ += " CASCADE";
        }
    }

    template <IsSqlDialect DialectT, Appendable StringT>
    constexpr void SqlGeneratorVisitor<DialectT, StringT>::visit_column_separator() {
        sql_ += ", ";
    }

}  // namespace demiplane::db
