// INSERT query expression tests
// Comprehensive tests for insert operations

#include <demiplane/scroll>

#include <postgres_dialect.hpp>
#include <query_compiler.hpp>

#include "common.hpp"

#include <gtest/gtest.h>

using namespace demiplane::db;


// Test fixture for INSERT operations
class InsertQueryTest : public ::testing::Test, public demiplane::scroll::LoggerProvider {
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

        // Create compiler
        compiler = std::make_unique<QueryCompiler>(std::make_unique<postgres::Dialect>(), false);
    }

    std::shared_ptr<Table> users_schema;
    std::unique_ptr<QueryCompiler> compiler;
};

// Test basic INSERT expression
TEST_F(InsertQueryTest, BasicInsertExpression) {
    auto query  = insert_into(users_schema).into({"name", "age", "active"}).values({std::string{"John Doe"}, 25, true});
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql().empty());
    SCROLL_LOG_INF() << result.sql();
}

// Test INSERT with table name string
TEST_F(InsertQueryTest, InsertWithTableNameExpression) {
    auto query  = insert_into("users").into({"name", "age"}).values({std::string{"Jane Doe"}, 30});
    auto result = compiler->compile(std::move(query));
    EXPECT_FALSE(result.sql().empty());
    SCROLL_LOG_INF() << result.sql();
}

// Test INSERT with Record
TEST_F(InsertQueryTest, InsertWithRecordExpression) {
    Record test_record(users_schema);
    test_record["name"].set(std::string("Bob Smith"));
    test_record["age"].set(35);
    test_record["active"].set(true);

    auto query  = insert_into(users_schema).into({"name", "age", "active"}).values(test_record);
    const auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql().empty());
    SCROLL_LOG_INF() << result.sql();
}

// Test INSERT batch operation
TEST_F(InsertQueryTest, InsertBatchExpression) {
    Record record1(users_schema);
    record1["name"].set(std::string("User1"));
    record1["age"].set(25);
    record1["active"].set(true);

    Record record2(users_schema);
    record2["name"].set(std::string("User2"));
    record2["age"].set(30);
    record2["active"].set(false);

    const std::vector records = {record1, record2};

    auto query  = insert_into(users_schema).into({"name", "age", "active"}).batch(records);
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql().empty());
    SCROLL_LOG_INF() << result.sql();
}

// Test INSERT multiple values calls
TEST_F(InsertQueryTest, InsertMultipleValuesExpression) {
    auto query = insert_into(users_schema)
                     .into({"name", "age", "active"})
                     .values({std::string{"User1"}, 25, true})
                     .values({std::string{"User2"}, 30, false});
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql().empty());
    SCROLL_LOG_INF() << result.sql();
}

// Test INSERT with empty columns (should work with schema inference)
TEST_F(InsertQueryTest, InsertEmptyColumnsExpression) {
    auto query = insert_into(users_schema);
    EXPECT_EQ(query.columns().size(), 0);
    EXPECT_EQ(query.rows().size(), 0);
}

// Test INSERT method chaining
TEST_F(InsertQueryTest, InsertMethodChainingExpression) {
    auto query = insert_into(users_schema).into({"name", "age", "active"});

    // Test that methods return reference for chaining
    auto& query_ref = query.values({std::string{"Test User"}, 40, true});
    EXPECT_EQ(&query, &query_ref);

    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql().empty());
    SCROLL_LOG_INF() << result.sql();
}
