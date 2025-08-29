// DELETE query expression tests
// Comprehensive tests for delete operations

#include <gtest/gtest.h>

#include "query_expressions.hpp"
#include "db_column.hpp"
#include "db_field_schema.hpp"
#include "db_table_schema.hpp"
#include "postgres_dialect.hpp"
#include "query_compiler.hpp"

#include <demiplane/scroll>

using namespace demiplane::db;

#define MANUAL_CHECK

// Test fixture for DELETE operations
class DeleteQueryTest : public ::testing::Test,
                        public demiplane::scroll::LoggerProvider {
protected:
    void SetUp() override {
        demiplane::scroll::FileLoggerConfig cfg;
        cfg.file                 = "query_test.log";
        cfg.add_time_to_filename = false;

        auto logger = std::make_shared<demiplane::scroll::FileLogger<demiplane::scroll::DetailedEntry>>(std::move(cfg));
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

// Test basic DELETE expression
TEST_F(DeleteQueryTest, BasicDeleteExpression) {
    auto query = delete_from(users_schema)
        .where(user_active == lit(false));
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());
    SCROLL_LOG_INF() << result.sql;
}

// Test DELETE with table name string
TEST_F(DeleteQueryTest, DeleteWithTableNameExpression) {
    auto query = delete_from("users")
        .where(user_id > lit(0));
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());
    SCROLL_LOG_INF() << result.sql;
}

// Test DELETE without WHERE clause
TEST_F(DeleteQueryTest, DeleteWithoutWhereExpression) {
    auto delete_query = delete_from(users_schema);
    auto result       = compiler->compile(delete_query);
    EXPECT_FALSE(result.sql.empty());
    SCROLL_LOG_INF() << result.sql;
}

// Test DELETE WHERE expression
TEST_F(DeleteQueryTest, DeleteWhereExpression) {
    const auto delete_query = delete_from(users_schema);
    auto query              = DeleteWhereExpr{delete_query, user_active == lit(false)};
    auto result       = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());
    SCROLL_LOG_INF() << result.sql;
}

// Test DELETE with complex WHERE conditions
TEST_F(DeleteQueryTest, DeleteComplexWhereExpression) {
    auto query = delete_from(users_schema)
        .where(user_active == lit(false) && user_age < lit(18));
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());
    SCROLL_LOG_INF() << result.sql;
}

// Test DELETE with IN condition
TEST_F(DeleteQueryTest, DeleteWithInExpression) {
    auto query = delete_from(users_schema)
        .where(in(user_age, lit(18), lit(19), lit(20)));
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());
    SCROLL_LOG_INF() << result.sql;
}

// Test DELETE with BETWEEN condition
TEST_F(DeleteQueryTest, DeleteWithBetweenExpression) {
    auto query = delete_from(users_schema)
        .where(between(user_age, lit(18), lit(25)));
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());
    SCROLL_LOG_INF() << result.sql;
}

// Test DELETE with subquery condition
TEST_F(DeleteQueryTest, DeleteWithSubqueryExpression) {
    auto inactive_users = select(user_id)
                          .from(users_schema)
                          .where(user_active == lit(false));

    auto query = delete_from(users_schema)
        .where(in(user_id, subquery(inactive_users)));
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());
    SCROLL_LOG_INF() << result.sql;
}