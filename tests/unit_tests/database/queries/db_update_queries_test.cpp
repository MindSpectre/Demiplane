// UPDATE query expression tests
// Comprehensive tests for update operations

#include <gtest/gtest.h>

#include "query_expressions.hpp"
#include "db_column.hpp"
#include "db_field_schema.hpp"
#include "db_table_schema.hpp"
#include "postgres_dialect.hpp"
#include "query_compiler.hpp"

#include <demiplane/scroll>

using namespace demiplane::db;


// Test fixture for UPDATE operations
class UpdateQueryTest : public ::testing::Test,
                        public demiplane::scroll::FileLoggerProvider<demiplane::scroll::DetailedEntry> {
protected:
    void SetUp() override {
        demiplane::scroll::FileLoggerConfig cfg;
        cfg.file             = "query_test.log";
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

// Test basic UPDATE expression
TEST_F(UpdateQueryTest, BasicUpdateExpression) {
    auto query = update(users_schema)
                 .set("active", false)
                 .where(user_age < lit(18));
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());
    SCROLL_LOG_INF() << result.sql;
}

// Test UPDATE with table name string
TEST_F(UpdateQueryTest, UpdateWithTableNameExpression) {
    auto query = update("users")
                 .set("active", true)
                 .where(user_id > lit(0));
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());
    SCROLL_LOG_INF() << result.sql;
}

// Test UPDATE with multiple set operations
TEST_F(UpdateQueryTest, UpdateMultipleSetExpression) {
    auto query = update(users_schema)
                 .set("active", false)
                 .set("age", 21)
                 .where(user_age < lit(18));
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());
    SCROLL_LOG_INF() << result.sql;
}

// Test UPDATE with initializer list set
TEST_F(UpdateQueryTest, UpdateInitializerListSetExpression) {
    auto query = update(users_schema)
                 .set({{"active", false}, {"age", 21}})
                 .where(user_age < lit(18));
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());
    SCROLL_LOG_INF() << result.sql;
}

// Test UPDATE without WHERE clause
TEST_F(UpdateQueryTest, UpdateWithoutWhereExpression) {
    auto update_query = update(users_schema).set("active", true);
    auto result       = compiler->compile(update_query);
    EXPECT_FALSE(result.sql.empty());
    SCROLL_LOG_INF() << result.sql;
}

// Test UPDATE WHERE expression
TEST_F(UpdateQueryTest, UpdateWhereExpression) {
    auto update_query = update(users_schema).set("active", false);
    auto query        = UpdateWhereExpr{update_query, user_age < lit(18)};
    auto result       = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());
    SCROLL_LOG_INF() << result.sql;
}

// Test UPDATE method chaining
TEST_F(UpdateQueryTest, UpdateMethodChainingExpression) {
    auto query = update(users_schema);

    // Test that methods return reference for chaining
    auto& query_ref = query.set("active", true);
    EXPECT_EQ(&query, &query_ref);

    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());
    SCROLL_LOG_INF() << result.sql;
}

// Test UPDATE with various field value types
TEST_F(UpdateQueryTest, UpdateVariousValueTypesExpression) {
    auto query = update(users_schema)
                 .set("name", std::string("New Name"))
                 .set("age", 30)
                 .set("active", true)
                 .where(user_id == lit(1));
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());
    SCROLL_LOG_INF() << result.sql;
}