// UPDATE query expression tests
// Comprehensive tests for update operations

#include <demiplane/scroll>

#include <postgres_dialect.hpp>
#include <query_compiler.hpp>

#include "common.hpp"

#include <gtest/gtest.h>

using namespace demiplane::db;


// Test fixture for UPDATE operations
class UpdateQueryTest : public ::testing::Test, public demiplane::scroll::LoggerProvider {
protected:
    void SetUp() override {
        demiplane::scroll::FileSinkConfig cfg;
        cfg.file                 = "query_test.log";
        cfg.add_time_to_filename = false;

        auto logger = std::make_shared<demiplane::scroll::Logger>();
        auto file_sink = std::make_shared<demiplane::scroll::FileSink<demiplane::scroll::DetailedEntry>>(std::move(cfg));
        logger->add_sink(std::move(file_sink));
        set_logger(std::move(logger));
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

// Test basic UPDATE expression
TEST_F(UpdateQueryTest, BasicUpdateExpression) {
    auto query  = update(users_schema).set("active", false).where(user_age < 18);
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql().empty());
    LOG_INF() << result.sql();
}

// Test UPDATE with table name string
TEST_F(UpdateQueryTest, UpdateWithTableNameExpression) {
    auto query  = update("users").set("active", true).where(user_id > 0);
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql().empty());
    LOG_INF() << result.sql();
}

// Test UPDATE with multiple set operations
TEST_F(UpdateQueryTest, UpdateMultipleSetExpression) {
    auto query  = update(users_schema).set("active", false).set("age", 21).where(user_age < 18);
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql().empty());
    LOG_INF() << result.sql();
}

// Test UPDATE with initializer list set
TEST_F(UpdateQueryTest, UpdateInitializerListSetExpression) {
    auto query = update(users_schema)
                     .set({
                         {"active", false},
                         {"age",    21   }
    })
                     .where(user_age < 18);
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql().empty());
    LOG_INF() << result.sql();
}

// Test UPDATE without WHERE clause
TEST_F(UpdateQueryTest, UpdateWithoutWhereExpression) {
    auto update_query = update(users_schema).set("active", true);
    auto result       = compiler->compile(update_query);
    EXPECT_FALSE(result.sql().empty());
    LOG_INF() << result.sql();
}

// Test UPDATE WHERE expression
TEST_F(UpdateQueryTest, UpdateWhereExpression) {
    const auto update_query = update(users_schema).set("active", false).where(user_age < 18);
    auto result             = compiler->compile(update_query);
    EXPECT_FALSE(result.sql().empty());
    LOG_INF() << result.sql();
}

// Test UPDATE method chaining
TEST_F(UpdateQueryTest, UpdateMethodChainingExpression) {
    auto query = update(users_schema);

    // Test that methods return reference for chaining
    auto& query_ref = query.set("active", true);
    EXPECT_EQ(&query, &query_ref);

    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql().empty());
    LOG_INF() << result.sql();
}

// Test UPDATE with various field value types
TEST_F(UpdateQueryTest, UpdateVariousValueTypesExpression) {
    auto query = update(users_schema)
                     .set("name", std::string("New Name"))
                     .set("age", 30)
                     .set("active", true)
                     .where(user_id == 1);
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql().empty());
    LOG_INF() << result.sql();
}
