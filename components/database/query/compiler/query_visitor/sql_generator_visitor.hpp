#pragma once

#include <memory>
#include <tuple>

#include <dialect_concepts.hpp>
#include <param_mode.hpp>
#include <sql_params.hpp>

#include "query_visitor.hpp"

namespace demiplane::db {

    template <IsSqlDialect DialectT, Appendable StringT = std::string, ParamMode Mode = ParamMode::Inline>
    class SqlGeneratorVisitor final : public QueryVisitor<SqlGeneratorVisitor<DialectT, StringT, Mode>> {
    public:
        // Constexpr path - no sink, no PMR
        constexpr SqlGeneratorVisitor() = default;

        // Runtime path - PMR string, no sink (Inline mode)
        explicit SqlGeneratorVisitor(std::pmr::memory_resource* mr)
            : sql_{mr} {
        }

        // Runtime path - with sink and PMR (Sink mode)
        SqlGeneratorVisitor(ParamSink* sink, std::pmr::memory_resource* mr)
            : sql_{mr},
              sink_{sink} {
        }

        template <typename Self>
        constexpr auto decompose(this Self&& self) {
            return std::make_tuple(std::forward<Self>(self).sql_, std::forward<Self>(self).param_count_);
        }

        // Get results
        template <typename Self>
        [[nodiscard]] constexpr auto sql(this Self&& self) {
            return std::forward<Self>(self).sql_;
        }

        [[nodiscard]] constexpr std::size_t param_count() const noexcept {
            return param_count_;
        }

        // ── Compile-time parameter collection helpers ──────────────
        template <typename T>
        constexpr auto capture_param([[maybe_unused]] const T& val) const noexcept {
            if constexpr (Mode == ParamMode::Tuple) {
                return std::make_tuple(val);
            } else {
                return std::tuple<>{};
            }
        }

        static constexpr auto no_params() noexcept {
            return std::tuple<>{};
        }

        template <typename... Tuples>
        constexpr auto cat_params(Tuples&&... tuples) const noexcept {
            if constexpr (Mode == ParamMode::Tuple) {
                return std::tuple_cat(std::forward<Tuples>(tuples)...);
            } else {
                return std::tuple<>{};
            }
        }

        // ALL visit_*_impl methods are PUBLIC - the CRTP base calls them via derived()
        constexpr void visit_column_impl(std::string_view name, std::string_view table, std::string_view alias);
        constexpr void visit_value_impl(const FieldValue& value);
        constexpr void visit_value_impl(FieldValue&& value);
        constexpr void visit_null_impl();
        constexpr void visit_param_placeholder_impl();

        constexpr void visit_all_columns_impl(std::string_view table);

        constexpr void visit_table_impl(const DynamicTablePtr& table);
        constexpr void visit_table_impl(std::string_view table_name);

        template <IsStaticTable TableTp>
        constexpr void visit_table_impl(const TableTp& table);

        constexpr void visit_alias_impl(std::string_view alias);

        // Expression helpers
        constexpr void visit_binary_expr_start();
        constexpr void visit_binary_expr_end();
        constexpr void visit_unary_expr_start() {
            gears::force_non_static(this);
        }
        constexpr void visit_unary_expr_end() {
            gears::force_non_static(this);
        }
        constexpr void visit_subquery_start();
        constexpr void visit_subquery_end();
        constexpr void visit_exists_start();
        constexpr void visit_exists_end();

        // Binary operators
        constexpr void visit_binary_op_impl(OpEqual);
        constexpr void visit_binary_op_impl(OpNotEqual);
        constexpr void visit_binary_op_impl(OpLess);
        constexpr void visit_binary_op_impl(OpLessEqual);
        constexpr void visit_binary_op_impl(OpGreater);
        constexpr void visit_binary_op_impl(OpGreaterEqual);
        constexpr void visit_binary_op_impl(OpAnd);
        constexpr void visit_binary_op_impl(OpOr);
        constexpr void visit_binary_op_impl(OpLike);
        constexpr void visit_binary_op_impl(OpNotLike);
        constexpr void visit_binary_op_impl(OpIn);

        // Unary operators
        constexpr void visit_unary_op_impl(OpNot);
        constexpr void visit_unary_op_impl(OpIsNull);
        constexpr void visit_unary_op_impl(OpIsNotNull);

        // Special operators
        constexpr void visit_between_impl();
        constexpr void visit_and_impl();
        constexpr void visit_in_list_start();
        constexpr void visit_in_list_end();
        constexpr void visit_in_list_separator();

        // Aggregate functions
        constexpr void visit_count_impl(bool distinct);
        constexpr void visit_sum_impl();
        constexpr void visit_avg_impl();
        constexpr void visit_max_impl();
        constexpr void visit_min_impl();
        constexpr void visit_aggregate_end(std::string_view alias);

        // Query parts
        constexpr void visit_select_start(bool distinct);
        constexpr void visit_select_end();
        constexpr void visit_from_start();
        constexpr void visit_from_end();
        constexpr void visit_where_start();
        constexpr void visit_where_end();
        constexpr void visit_group_by_start();
        constexpr void visit_group_by_end();
        constexpr void visit_having_start();
        constexpr void visit_having_end();
        constexpr void visit_order_by_start();
        constexpr void visit_order_by_end();
        constexpr void visit_order_direction_impl(OrderDirection dir);
        constexpr void visit_limit_impl(std::size_t limit, std::size_t offset);

        // Joins
        constexpr void visit_join_start(JoinType type);
        constexpr void visit_join_on();
        constexpr void visit_join_end();

        // DML
        constexpr void visit_insert_start();
        constexpr void visit_insert_columns(const std::vector<std::string>& columns);
        constexpr void visit_insert_columns(std::vector<std::string>&& columns);
        constexpr void visit_insert_values(const std::vector<std::vector<FieldValue>>& rows);
        constexpr void visit_insert_values(std::vector<std::vector<FieldValue>>&& rows);
        constexpr void visit_insert_end();

        constexpr void visit_update_start();
        constexpr void visit_update_set(const std::vector<std::pair<std::string, FieldValue>>& assignments);
        constexpr void visit_update_set(std::vector<std::pair<std::string, FieldValue>>&& assignments);
        constexpr void visit_update_end();

        constexpr void visit_delete_start();
        constexpr void visit_delete_end();

        constexpr void visit_case_start();
        constexpr void visit_case_end();
        constexpr void visit_when_start();
        constexpr void visit_when_then();
        constexpr void visit_when_end();
        constexpr void visit_else_start();
        constexpr void visit_else_end();

        // CTE (Common Table Expression)
        constexpr void visit_cte_start(bool recursive);
        constexpr void visit_cte_name_impl(std::string_view name);
        constexpr void visit_cte_as_start();
        constexpr void visit_cte_as_end();
        constexpr void visit_cte_end();

        // DDL - CREATE TABLE
        constexpr void visit_create_table_start(bool if_not_exists);
        constexpr void visit_create_table_columns(const DynamicTablePtr& table);

        template <IsStaticTable TableTp>
        constexpr void visit_create_table_columns(const TableTp& table);

        constexpr void visit_create_table_end();

        // DDL - DROP TABLE
        constexpr void visit_drop_table_start(bool if_exists);
        constexpr void visit_drop_table_end(bool cascade);

        // Set operations
        constexpr void visit_set_op_impl(SetOperation op);

        // Column separator
        constexpr void visit_column_separator();

    private:
        StringT sql_{};
        ParamSink* sink_         = nullptr;
        std::size_t param_count_ = 0;
    };
}  // namespace demiplane::db

#include "detail/sql_generator_visitor.inl"
