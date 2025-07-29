#pragma once

#include "query_visitor.hpp"
#include "../../../dialects/interface/sql_dialect.hpp"

namespace demiplane::db {
    class SqlGeneratorVisitor final : public QueryVisitor {
    public:
        explicit SqlGeneratorVisitor(std::shared_ptr<SqlDialect> dialect, bool use_params = true);

        // Get results
        std::string sql() const {
            return sql_.str();
        }

        const std::vector<FieldValue>& parameters() const {
            return parameters_;
        }

    protected:
        // Column and value implementations
        void visit_column_impl(const FieldSchema* schema, const char* table, const char* alias) override;

        void visit_literal_impl(const FieldValue& value) override;

        void visit_null_impl() override;

        void visit_all_columns_impl(const char* table) override;

        void visit_parameter_impl(std::size_t index, std::type_index type) override;

        void visit_table_impl(const TableSchemaPtr& table) override;

        void visit_alias_impl(const char* alias) override;

        void visit_table_alias_impl(const char* alias) override;

        // Expression helpers
        void visit_binary_expr_start() override;

        void visit_binary_expr_end() override;

        void visit_subquery_start() override;

        void visit_subquery_end() override;

        void visit_exists_start() override;

        void visit_exists_end() override;

        // Binary operators
        void visit_binary_op_impl(OpEqual) override;

        void visit_binary_op_impl(OpNotEqual) override;

        void visit_binary_op_impl(OpLess) override;

        void visit_binary_op_impl(OpLessEqual) override;

        void visit_binary_op_impl(OpGreater) override;

        void visit_binary_op_impl(OpGreaterEqual) override;

        void visit_binary_op_impl(OpAnd) override;

        void visit_binary_op_impl(OpOr) override;

        void visit_binary_op_impl(OpLike) override;

        void visit_binary_op_impl(OpNotLike) override;

        // Unary operators
        void visit_unary_op_impl(OpNot) override;

        void visit_unary_op_impl(OpIsNull) override;

        void visit_unary_op_impl(OpIsNotNull) override;

        // Special operators
        void visit_between_impl() override;

        void visit_and_impl() override;

        void visit_in_list_start() override;

        void visit_in_list_end() override;

        void visit_in_list_separator() override;

        // Aggregate functions
        void visit_count_impl(bool distinct, const char* alias) override;

        void visit_sum_impl(const char* alias) override;

        void visit_avg_impl(const char* alias) override;

        void visit_max_impl(const char* alias) override;

        void visit_min_impl(const char* alias) override;

        void visit_aggregate_end() override;

        // Query parts
        void visit_select_start(bool distinct) override;

        void visit_select_end() override;

        void visit_from_start() override;

        void visit_from_end() override;

        void visit_where_start() override;

        void visit_where_end() override;

        void visit_group_by_start() override;

        void visit_group_by_end() override;

        void visit_having_start() override;

        void visit_having_end() override;

        void visit_order_by_start() override;

        void visit_order_by_end() override;

        void visit_order_direction_impl(OrderDirection dir) override;

        void visit_limit_impl(std::size_t limit, std::size_t offset) override;

        // Joins
        void visit_join_start(JoinType type) override;

        void visit_join_on() override;

        void visit_join_end() override;

        // DML
        void visit_insert_start() override;

        void visit_insert_columns(const std::vector<std::string>& columns) override;

        void visit_insert_values(const std::vector<std::vector<FieldValue>>& rows) override;

        void visit_insert_end() override;

        void visit_update_start() override;

        void visit_update_set(const std::vector<std::pair<std::string, FieldValue>>& assignments) override;

        void visit_update_end() override;

        void visit_delete_start() override;

        void visit_delete_end() override;

        void visit_case_start() override;

        void visit_case_end() override;

        void visit_when_start() override;

        void visit_when_then() override;

        void visit_when_end() override;

        void visit_else_start() override;

        void visit_else_end() override;

        // Set operations
        void visit_set_op_impl(SetOperation op) override;

        // Column separator
        void visit_column_separator() override;

    private:
        std::shared_ptr<SqlDialect> dialect_;
        std::ostringstream sql_;
        std::vector<FieldValue> parameters_;
        bool use_parameters_;
    };
}
