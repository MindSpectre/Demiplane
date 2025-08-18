// INSERT query expression tests
// Comprehensive tests for insert operations

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

// Test fixture for INSERT operations
class InsertQueryTest : public ::testing::Test,
                        public demiplane::scroll::FileLoggerProvider {
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

        // Create compiler
        compiler = std::make_unique<QueryCompiler>(std::make_unique<PostgresDialect>(), false);
    }

    std::shared_ptr<TableSchema> users_schema;
    std::unique_ptr<QueryCompiler> compiler;
};

// Test basic INSERT expression
TEST_F(InsertQueryTest, BasicInsertExpression) {
    auto query = insert_into(users_schema)
                 .into({"name", "age", "active"})
                 .values({"John Doe", 25, true});
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());
    SCROLL_LOG_INF() << result.sql;
}

// Test INSERT with table name string
TEST_F(InsertQueryTest, InsertWithTableNameExpression) {
    auto query = insert_into("users")
                 .into({"name", "age"})
                 .values({"Jane Doe", 30});
    auto result = compiler->compile(std::move(query));
    EXPECT_FALSE(result.sql.empty());
    SCROLL_LOG_INF() << result.sql;
}

// Test INSERT with Record
TEST_F(InsertQueryTest, InsertWithRecordExpression) {
    Record test_record(users_schema);
    test_record["name"].set(std::string("Bob Smith"));
    test_record["age"].set(35);
    test_record["active"].set(true);

    auto query = insert_into(users_schema)
                 .into({"name", "age", "active"})
                 .values(test_record);
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());
    SCROLL_LOG_INF() << result.sql;
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

    std::vector<Record> records = {record1, record2};

    auto query = insert_into(users_schema)
                 .into({"name", "age", "active"})
                 .batch(records);
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());
    SCROLL_LOG_INF() << result.sql;
}

// Test INSERT multiple values calls
TEST_F(InsertQueryTest, InsertMultipleValuesExpression) {
    auto query = insert_into(users_schema)
                 .into({"name", "age", "active"})
                 .values({"User1", 25, true})
                 .values({"User2", 30, false});
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());
    SCROLL_LOG_INF() << result.sql;
}

// Test INSERT with empty columns (should work with schema inference)
TEST_F(InsertQueryTest, InsertEmptyColumnsExpression) {
    auto query = insert_into(users_schema);
    EXPECT_EQ(query.columns().size(), 0);
    EXPECT_EQ(query.rows().size(), 0);
}

// Test INSERT method chaining
TEST_F(InsertQueryTest, InsertMethodChainingExpression) {
    auto query = insert_into(users_schema)
        .into({"name", "age", "active"});

    // Test that methods return reference for chaining
    auto& query_ref = query.values({"Test User", 40, true});
    EXPECT_EQ(&query, &query_ref);

    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());
    SCROLL_LOG_INF() << result.sql;
}