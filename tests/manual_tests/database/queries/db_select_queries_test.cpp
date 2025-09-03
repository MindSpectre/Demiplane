// SELECT query expression tests
// Comprehensive tests for select operations and selectable types

#include <demiplane/scroll>

#include "db_column.hpp"
#include "db_field_schema.hpp"
#include "db_table_schema.hpp"
#include "postgres_dialect.hpp"
#include "query_compiler.hpp"
#include "query_expressions.hpp"

#include <gtest/gtest.h>

using namespace demiplane::db;


// Test fixture for SELECT operations
class SelectQueryTest : public ::testing::Test, public demiplane::scroll::LoggerProvider {
protected:
    void SetUp() override {
        demiplane::scroll::FileLoggerConfig cfg;
        cfg.file                 = "query_test.log";
        cfg.add_time_to_filename = false;

        auto logger = std::make_shared<demiplane::scroll::FileLogger<demiplane::scroll::DetailedEntry>>(std::move(cfg));
        set_logger(std::move(logger));
        // Create test schemas
        users_schema = std::make_shared<TableSchema>("users");
        users_schema->add_field<int>("id", "INTEGER")
            .primary_key("id")
            .add_field<std::string>("name", "VARCHAR(255)")
            .add_field<int>("age", "INTEGER")
            .add_field<bool>("active", "BOOLEAN");

        posts_schema = std::make_shared<TableSchema>("posts");
        posts_schema->add_field<int>("id", "INTEGER")
            .primary_key("id")
            .add_field<int>("user_id", "INTEGER")
            .add_field<std::string>("title", "VARCHAR(255)")
            .add_field<bool>("published", "BOOLEAN");

        // Create column references
        user_id     = users_schema->column<int>("id");
        user_name   = users_schema->column<std::string>("name");
        user_age    = users_schema->column<int>("age");
        user_active = users_schema->column<bool>("active");

        post_id        = posts_schema->column<int>("id");
        post_user_id   = posts_schema->column<int>("user_id");
        post_title     = posts_schema->column<std::string>("title");
        post_published = posts_schema->column<bool>("published");

        // Create compiler
        compiler = std::make_unique<QueryCompiler>(std::make_unique<PostgresDialect>(), false);
    }

    std::shared_ptr<TableSchema> users_schema;
    std::shared_ptr<TableSchema> posts_schema;

    TableColumn<int> user_id{nullptr, ""};
    TableColumn<std::string> user_name{nullptr, ""};
    TableColumn<int> user_age{nullptr, ""};
    TableColumn<bool> user_active{nullptr, ""};

    TableColumn<int> post_id{nullptr, ""};
    TableColumn<int> post_user_id{nullptr, ""};
    TableColumn<std::string> post_title{nullptr, ""};
    TableColumn<bool> post_published{nullptr, ""};

    std::unique_ptr<QueryCompiler> compiler;
};

// Test basic SELECT expression
TEST_F(SelectQueryTest, BasicSelectExpression) {
    const auto query = select(user_id, user_name);
    auto result      = compiler->compile(query.from(users_schema));
    EXPECT_FALSE(result.sql.empty());
    SCROLL_LOG_INF() << result.sql;
}

// Test SELECT with ALL columns
TEST_F(SelectQueryTest, SelectAllColumnsExpression) {
    const auto query = select(all("users"));
    auto result      = compiler->compile(query.from(users_schema));
    EXPECT_FALSE(result.sql.empty());
    SCROLL_LOG_INF() << result.sql;
}

// Test SELECT DISTINCT
TEST_F(SelectQueryTest, SelectDistinctExpression) {
    const auto query = select_distinct(user_name, user_age);
    auto result      = compiler->compile(query.from(users_schema));
    EXPECT_FALSE(result.sql.empty());
    SCROLL_LOG_INF() << result.sql;
}

// Test SELECT with mixed types (columns, literals, aggregates)
TEST_F(SelectQueryTest, SelectMixedTypesExpression) {
    const auto query = select(user_name, lit("constant"), count(user_id).as("total"));
    auto result      = compiler->compile(query.from(users_schema));
    EXPECT_FALSE(result.sql.empty());
    SCROLL_LOG_INF() << result.sql;
}

// Test SELECT from Record
TEST_F(SelectQueryTest, SelectFromRecordExpression) {
    Record test_record(users_schema);
    test_record["id"].set(1);
    test_record["name"].set(std::string("test"));

    auto query  = select(user_name).from(test_record);
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());
    SCROLL_LOG_INF() << result.sql;
}

// Test SELECT from table name string
TEST_F(SelectQueryTest, SelectFromTableNameExpression) {
    auto query  = select(lit(1)).from("test_table");
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());
    SCROLL_LOG_INF() << result.sql;
}

// Test SELECT with WHERE clause
TEST_F(SelectQueryTest, SelectWithWhereExpression) {
    auto query  = select(user_name).from(users_schema).where(user_age > lit(18));
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());
    SCROLL_LOG_INF() << result.sql;
}

// Test SELECT with JOIN
TEST_F(SelectQueryTest, SelectWithJoinExpression) {
    auto query =
        select(user_name, post_title).from(users_schema).join(posts_schema->table_name()).on(post_user_id == user_id);
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());
    SCROLL_LOG_INF() << result.sql;
}

// Test SELECT with GROUP BY
TEST_F(SelectQueryTest, SelectWithGroupByExpression) {
    auto query  = select(user_active, count(user_id).as("user_count")).from(users_schema).group_by(user_active);
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());
    SCROLL_LOG_INF() << result.sql;
}

// Test SELECT with HAVING
TEST_F(SelectQueryTest, SelectWithHavingExpression) {
    auto query = select(user_active, count(user_id).as("user_count"))
                     .from(users_schema)
                     .group_by(user_active)
                     .having(count(user_id) > lit(5));
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());
    SCROLL_LOG_INF() << result.sql;
}

// Test SELECT with ORDER BY
TEST_F(SelectQueryTest, SelectWithOrderByExpression) {
    auto query  = select(user_name).from(users_schema).order_by(asc(user_name));
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());
    SCROLL_LOG_INF() << result.sql;
}

// Test SELECT with LIMIT
TEST_F(SelectQueryTest, SelectWithLimitExpression) {
    auto query  = select(user_name).from(users_schema).limit(10);
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());
    SCROLL_LOG_INF() << result.sql;
}
