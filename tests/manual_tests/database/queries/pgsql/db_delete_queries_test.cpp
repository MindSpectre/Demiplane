// DELETE query expression tests
// Comprehensive tests for delete operations

#include <demiplane/scroll>

#include <postgres_dialect.hpp>
#include <query_compiler.hpp>

#include "common.hpp"

#include <gtest/gtest.h>

using namespace demiplane::db;

// Test fixture for DELETE operations
class DeleteQueryTest : public ::testing::Test, public demiplane::scroll::LoggerProvider {
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
        compiler = std::make_unique<QueryCompiler>(std::make_unique<postgres::Dialect>(), false);
    }

    std::shared_ptr<Table> users_schema;

    TableColumn<int> user_id{nullptr, ""};
    TableColumn<std::string> user_name{nullptr, ""};
    TableColumn<int> user_age{nullptr, ""};
    TableColumn<bool> user_active{nullptr, ""};

    std::unique_ptr<QueryCompiler> compiler;
};

// Test basic DELETE expression
TEST_F(DeleteQueryTest, BasicDeleteExpression) {
    auto query  = delete_from(users_schema).where(user_active == false);
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql().empty());
    SCROLL_LOG_INF() << result.sql();
}

// Test DELETE with table name string
TEST_F(DeleteQueryTest, DeleteWithTableNameExpression) {
    auto query  = delete_from("users").where(user_id > 0);
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql().empty());
    SCROLL_LOG_INF() << result.sql();
}

// Test DELETE without WHERE clause
TEST_F(DeleteQueryTest, DeleteWithoutWhereExpression) {
    auto delete_query = delete_from(users_schema);
    auto result       = compiler->compile(delete_query);
    EXPECT_FALSE(result.sql().empty());
    SCROLL_LOG_INF() << result.sql();
}

// Test DELETE WHERE expression
TEST_F(DeleteQueryTest, DeleteWhereExpression) {
    const auto delete_query = delete_from(users_schema).where(user_active == false);
    auto result             = compiler->compile(delete_query);
    EXPECT_FALSE(result.sql().empty());
    SCROLL_LOG_INF() << result.sql();
}

// Test DELETE with complex WHERE conditions
TEST_F(DeleteQueryTest, DeleteComplexWhereExpression) {
    auto query  = delete_from(users_schema).where(user_active == false && user_age < 18);
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql().empty());
    SCROLL_LOG_INF() << result.sql();
}

// Test DELETE with IN condition
TEST_F(DeleteQueryTest, DeleteWithInExpression) {
    auto query  = delete_from(users_schema).where(in(user_age, 18, 19, 20));
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql().empty());
    SCROLL_LOG_INF() << result.sql();
}

// Test DELETE with BETWEEN condition
TEST_F(DeleteQueryTest, DeleteWithBetweenExpression) {
    auto query  = delete_from(users_schema).where(between(user_age, 18, 25));
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql().empty());
    SCROLL_LOG_INF() << result.sql();
}

// Test DELETE with subquery condition
TEST_F(DeleteQueryTest, DeleteWithSubqueryExpression) {
    auto inactive_users = select(user_id).from(users_schema).where(user_active == false);

    auto query  = delete_from(users_schema).where(in(user_id, subquery(inactive_users)));
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql().empty());
    SCROLL_LOG_INF() << result.sql();
}
