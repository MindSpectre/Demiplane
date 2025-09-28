// Aggregate query expression tests
// Comprehensive tests for aggregate functions and expressions

#include <demiplane/nexus>
#include <demiplane/scroll>

#include "common.hpp"
#include "postgres_dialect.hpp"
#include "query_compiler.hpp"
#include "query_expressions.hpp"

#include <gtest/gtest.h>

using namespace demiplane::db;


// Test fixture for aggregate operations
class AggregateQueryTest : public ::testing::Test, public demiplane::scroll::LoggerProvider {
protected:
    void SetUp() override {
        SET_COMMON_LOGGER();
        // Create test schema
        users_schema = std::make_shared<Table>("users");
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

    std::shared_ptr<Table> users_schema;

    TableColumn<int> user_id{nullptr, ""};
    TableColumn<std::string> user_name{nullptr, ""};
    TableColumn<int> user_age{nullptr, ""};
    TableColumn<bool> user_active{nullptr, ""};

    std::unique_ptr<QueryCompiler> compiler;
};

// Test basic aggregate expressions
TEST_F(AggregateQueryTest, BasicAggregateExpressions) {
    SCROLL_LOG_INF() << "Aggregate expressions:";
    {
        // COUNT
        auto count_query = select(count(user_id)).from(users_schema);
        auto q           = compiler->compile(count_query);
        const auto sql   = q.sql();
        EXPECT_FALSE(sql.empty());
        SCROLL_LOG_INF() << "COUNT: " << sql;
    }

    // SUM
    {
        auto sum_query = select(sum(user_age)).from(users_schema);
        auto q         = compiler->compile(sum_query);
        const auto sql = q.sql();
        EXPECT_FALSE(sql.empty());
        SCROLL_LOG_INF() << "SUM: " << sql;
    }

    // AVG
    {
        auto avg_query = select(avg(user_age)).from(users_schema);
        auto q         = compiler->compile(avg_query);
        const auto sql = q.sql();
        EXPECT_FALSE(sql.empty());
        SCROLL_LOG_INF() << "AVG: " << sql;
    }

    // MIN
    {
        auto min_query = select(min(user_age)).from(users_schema);
        auto q         = compiler->compile(min_query);
        const auto sql = q.sql();
        EXPECT_FALSE(sql.empty());
        SCROLL_LOG_INF() << "MIN: " << sql;
    }
    // MAX
    {
        auto max_query = select(max(user_age)).from(users_schema);
        auto q         = compiler->compile(max_query);
        const auto sql = q.sql();
        EXPECT_FALSE(sql.empty());
        SCROLL_LOG_INF() << "MAX: " << sql;
    }
}

// Test aggregate expressions with aliases
TEST_F(AggregateQueryTest, AggregateWithAliasExpressions) {
    auto query = select(count(user_id).as("total_users"),
                        sum(user_age).as("total_age"),
                        avg(user_age).as("avg_age"),
                        min(user_age).as("min_age"),
                        max(user_age).as("max_age"))
                     .from(users_schema);

    const auto q   = compiler->compile(query);
    const auto sql = q.sql();
    EXPECT_FALSE(sql.empty());

    SCROLL_LOG_INF() << sql;
}

// Test COUNT DISTINCT
TEST_F(AggregateQueryTest, CountDistinctExpression) {
    auto query     = select(count_distinct(user_age)).from(users_schema);
    const auto q   = compiler->compile(query);
    const auto sql = q.sql();
    EXPECT_FALSE(sql.empty());

    SCROLL_LOG_INF() << sql;
}

// Test COUNT ALL
TEST_F(AggregateQueryTest, CountAllExpression) {
    auto query     = select(count_all()).from(users_schema);
    const auto q   = compiler->compile(query);
    const auto sql = q.sql();
    EXPECT_FALSE(sql.empty());

    SCROLL_LOG_INF() << sql;
}

// Test aggregates with GROUP BY
TEST_F(AggregateQueryTest, AggregateWithGroupByExpression) {
    auto query     = select(user_active, count(user_id).as("user_count")).from(users_schema).group_by(user_active);
    const auto q   = compiler->compile(query);
    const auto sql = q.sql();
    EXPECT_FALSE(sql.empty());

    SCROLL_LOG_INF() << sql;
}

// Test aggregates with HAVING
TEST_F(AggregateQueryTest, AggregateWithHavingExpression) {
    auto query = select(user_active, count(user_id).as("user_count"))
                     .from(users_schema)
                     .group_by(user_active)
                     .having(count(user_id) > 5);
    const auto q   = compiler->compile(query);
    const auto sql = q.sql();
    EXPECT_FALSE(sql.empty());

    SCROLL_LOG_INF() << sql;
}

// Test multiple aggregates in same query
TEST_F(AggregateQueryTest, MultipleAggregatesExpression) {
    auto query =
        select(count(user_id), sum(user_age), avg(user_age), min(user_age), max(user_age), count_distinct(user_name))
            .from(users_schema);

    const auto q   = compiler->compile(std::move(query));
    const auto sql = q.sql();
    EXPECT_FALSE(sql.empty());

    SCROLL_LOG_INF() << sql;
}

// Test aggregate with mixed column types
TEST_F(AggregateQueryTest, AggregateWithMixedTypesExpression) {
    auto query = select(user_name, count(user_id).as("count"), "literal_value", avg(user_age).as("avg_age"))
                     .from(users_schema)
                     .group_by(user_name);

    const auto q   = compiler->compile(query);
    const auto sql = q.sql();
    EXPECT_FALSE(sql.empty());

    SCROLL_LOG_INF() << sql;
}

// Test aggregate method chaining
TEST_F(AggregateQueryTest, AggregateMethodChainingExpression) {
    auto count_expr = count(user_id);
    auto& alias_ref = count_expr.as("user_count");
    EXPECT_EQ(&count_expr, &alias_ref);

    auto query     = select(count_expr).from(users_schema);
    const auto q   = compiler->compile(query);
    const auto sql = q.sql();
    EXPECT_FALSE(sql.empty());

    SCROLL_LOG_INF() << sql;
}
