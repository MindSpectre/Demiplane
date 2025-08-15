// Aggregate query expression tests
// Comprehensive tests for aggregate functions and expressions

#include <gtest/gtest.h>

#include "query_expressions.hpp"
#include "db_column.hpp"
#include "db_field_schema.hpp"
#include "db_table_schema.hpp"
#include "postgres_dialect.hpp"
#include "query_compiler.hpp"

#include <demiplane/scroll>

using namespace demiplane::db;


// Test fixture for aggregate operations
class AggregateQueryTest : public ::testing::Test,
                           public demiplane::scroll::FileLoggerProvider<demiplane::scroll::DetailedEntry> {
protected:
    void SetUp() override {
        demiplane::scroll::FileLoggerConfig cfg;
        cfg.file = "query_test.log";
        cfg.add_time_to_name = false;

        std::shared_ptr<demiplane::scroll::FileLogger<demiplane::scroll::DetailedEntry>> logger = std::make_shared<
            demiplane::scroll::FileLogger<demiplane::scroll::DetailedEntry>>(std::move(cfg));
        set_logger(std::move(logger));
        // Create test schema
        users_schema = std::make_shared<TableSchema>("users");
        users_schema->add_field<int>("id", "INTEGER")
                    .primary_key("id")
                    .add_field<std::string>("name", "VARCHAR(255)")
                    .add_field<int>("age", "INTEGER")
                    .add_field<bool>("active", "BOOLEAN");

        // Create column references
        user_id     = users_schema->column<int>("id");
        user_name   = users_schema->column<std::string>("name");
        user_age    = users_schema->column<int>("age");
        user_active = users_schema->column<bool>("active");

        // Create compiler
        compiler = std::make_unique<QueryCompiler>(std::make_unique<PostgresDialect>(), false);
    }

    std::shared_ptr<TableSchema> users_schema;

    TableColumn<int> user_id{nullptr, ""};
    TableColumn<std::string> user_name{nullptr, ""};
    TableColumn<int> user_age{nullptr, ""};
    TableColumn<bool> user_active{nullptr, ""};

    std::unique_ptr<QueryCompiler> compiler;
};

// Test basic aggregate expressions
TEST_F(AggregateQueryTest, BasicAggregateExpressions) {
    // COUNT
    auto count_query  = select(count(user_id)).from(users_schema);
    auto count_result = compiler->compile(count_query);
    EXPECT_FALSE(count_result.sql.empty());

    // SUM
    auto sum_query  = select(sum(user_age)).from(users_schema);
    auto sum_result = compiler->compile(sum_query);
    EXPECT_FALSE(sum_result.sql.empty());

    // AVG
    auto avg_query  = select(avg(user_age)).from(users_schema);
    auto avg_result = compiler->compile(avg_query);
    EXPECT_FALSE(avg_result.sql.empty());

    // MIN
    auto min_query  = select(min(user_age)).from(users_schema);
    auto min_result = compiler->compile(min_query);
    EXPECT_FALSE(min_result.sql.empty());

    // MAX
    auto max_query  = select(max(user_age)).from(users_schema);
    auto max_result = compiler->compile(max_query);
    EXPECT_FALSE(max_result.sql.empty());


    SCROLL_LOG_INF() << "Aggregate expressions:";
    SCROLL_LOG_INF() << "COUNT: " << count_result.sql;
    SCROLL_LOG_INF() << "SUM: " << sum_result.sql;
    SCROLL_LOG_INF() << "AVG: " << avg_result.sql;
    SCROLL_LOG_INF() << "MIN: " << min_result.sql;
    SCROLL_LOG_INF() << "MAX: " << max_result.sql;
}

// Test aggregate expressions with aliases
TEST_F(AggregateQueryTest, AggregateWithAliasExpressions) {
    auto query = select(
        count(user_id).as("total_users"),
        sum(user_age).as("total_age"),
        avg(user_age).as("avg_age"),
        min(user_age).as("min_age"),
        max(user_age).as("max_age")
    ).from(users_schema);

    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());

    SCROLL_LOG_INF() << result.sql;
}

// Test COUNT DISTINCT
TEST_F(AggregateQueryTest, CountDistinctExpression) {
    auto query  = select(count_distinct(user_age)).from(users_schema);
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());

    SCROLL_LOG_INF() << result.sql;
}

// Test COUNT ALL
TEST_F(AggregateQueryTest, CountAllExpression) {
    auto query  = select(count_all(users_schema->table_name())).from(users_schema);
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());

    SCROLL_LOG_INF() << result.sql;
}

// Test aggregates with GROUP BY
TEST_F(AggregateQueryTest, AggregateWithGroupByExpression) {
    auto query = select(user_active, count(user_id).as("user_count"))
                 .from(users_schema)
                 .group_by(user_active);
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());

    SCROLL_LOG_INF() << result.sql;
}

// Test aggregates with HAVING
TEST_F(AggregateQueryTest, AggregateWithHavingExpression) {
    auto query = select(user_active, count(user_id).as("user_count"))
                 .from(users_schema)
                 .group_by(user_active)
                 .having(count(user_id) > lit(5));
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());

    SCROLL_LOG_INF() << result.sql;
}

// Test multiple aggregates in same query
TEST_F(AggregateQueryTest, MultipleAggregatesExpression) {
    auto query = select(
        count(user_id),
        sum(user_age),
        avg(user_age),
        min(user_age),
        max(user_age),
        count_distinct(user_name)
    ).from(users_schema);

    auto result = compiler->compile(std::move(query));
    EXPECT_FALSE(result.sql.empty());

    SCROLL_LOG_INF() << result.sql;
}

// Test aggregate with mixed column types
TEST_F(AggregateQueryTest, AggregateWithMixedTypesExpression) {
    auto query = select(
            user_name,
            count(user_id).as("count"),
            lit("literal_value"),
            avg(user_age).as("avg_age")
        ).from(users_schema)
         .group_by(user_name);

    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());

    SCROLL_LOG_INF() << result.sql;
}

// Test aggregate method chaining
TEST_F(AggregateQueryTest, AggregateMethodChainingExpression) {
    auto count_expr = count(user_id);
    auto& alias_ref = count_expr.as("user_count");
    EXPECT_EQ(&count_expr, &alias_ref);

    auto query  = select(count_expr).from(users_schema);
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());

    SCROLL_LOG_INF() << result.sql;
}
