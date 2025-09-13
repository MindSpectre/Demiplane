#pragma once

#include "query_visitor.hpp"

namespace demiplane::db {
    class SqlDialect;

    class SqlGeneratorVisitor final : public QueryVisitor {
    public:
        explicit SqlGeneratorVisitor(std::shared_ptr<SqlDialect> dialect, bool use_params = true);

        template <typename Self>
        auto decompose(this Self&& self) {
            return std::make_tuple(std::forward<Self>(self).sql_.str(), std::forward<Self>(self).parameters_);
        }

        // Get results
        template <typename Self>
        [[nodiscard]] auto sql(this Self&& self) {
            return std::forward<Self>(self).sql_.str();
        }

        template <typename Self>
        [[nodiscard]] auto&& parameters(this Self&& self) {
            return std::forward<Self>(self).parameters_;
        }

    protected:
        // Column and value implementations
        void visit_table_column_impl(const FieldSchema* schema,
                                     const std::shared_ptr<std::string>& table,
                                     const std::optional<std::string>& alias) override;
        void visit_dynamic_column_impl(const std::string& name, const std::optional<std::string>& table) override;
        void visit_value_impl(const FieldValue& value) override;
        void visit_value_impl(FieldValue&& value) override;
        void visit_null_impl() override;

        void visit_all_columns_impl(const std::shared_ptr<std::string>& table) override;

        void visit_table_impl(const TablePtr& table) override;
        void visit_table_impl(std::string_view table_name) override;
        void visit_table_impl(const std::shared_ptr<std::string>& table) override;

        void visit_alias_impl(std::string_view alias) override;
        void visit_alias_impl(const std::optional<std::string>& alias) override;
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

        void visit_binary_op_impl(OpIn) override;
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
        void visit_count_impl(bool distinct) override;

        void visit_sum_impl() override;

        void visit_avg_impl() override;

        void visit_max_impl() override;

        void visit_min_impl() override;

        void visit_aggregate_end(const std::optional<std::string>& alias) override;
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
        void visit_insert_columns(std::vector<std::string>&& columns) override;

        void visit_insert_values(const std::vector<std::vector<FieldValue>>& rows) override;
        void visit_insert_values(std::vector<std::vector<FieldValue>>&& rows) override;

        void visit_insert_end() override;

        void visit_update_start() override;

        void visit_update_set(const std::vector<std::pair<std::string, FieldValue>>& assignments) override;
        void visit_update_set(std::vector<std::pair<std::string, FieldValue>>&& assignments) override;

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

        // CTE (Common Table Expression)
        void visit_cte_start(bool recursive) override;

        void visit_cte_name_impl(std::string_view name) override;

        void visit_cte_as_start() override;

        void visit_cte_as_end() override;

        void visit_cte_end() override;

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
}  // namespace demiplane::db
