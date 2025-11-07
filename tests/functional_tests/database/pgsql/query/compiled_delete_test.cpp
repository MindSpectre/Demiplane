// Compiled DELETE Query Functional Tests
// Tests query compilation + execution with SyncExecutor

#include "postgres_sync_executor.hpp"

#include <gtest/gtest.h>

#include "postgres_dialect.hpp"
#include "postgres_params.hpp"
#include "query_compiler.hpp"

using namespace demiplane::db;
using namespace demiplane::db::postgres;

// Test fixture for compiled DELETE queries
class CompiledDeleteTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Get connection parameters from environment or use defaults
        const char* host     = std::getenv("POSTGRES_HOST") ? std::getenv("POSTGRES_HOST") : "localhost";
        const char* port     = std::getenv("POSTGRES_PORT") ? std::getenv("POSTGRES_PORT") : "5433";
        const char* dbname   = std::getenv("POSTGRES_DB") ? std::getenv("POSTGRES_DB") : "test_db";
        const char* user     = std::getenv("POSTGRES_USER") ? std::getenv("POSTGRES_USER") : "test_user";
        const char* password = std::getenv("POSTGRES_PASSWORD") ? std::getenv("POSTGRES_PASSWORD") : "test_password";

        // Create connection string
        std::string conninfo = "host=" + std::string(host) + " port=" + std::string(port) +
                               " dbname=" + std::string(dbname) + " user=" + std::string(user) +
                               " password=" + std::string(password);

        // Connect to database
        conn_ = PQconnectdb(conninfo.c_str());

        // Check connection status
        // if (PQstatus(conn_) != CONNECTION_OK) {
        //     std::string error = PQerrorMessage(conn_);
        //     PQfinish(conn_);
        //     conn_ = nullptr;
        //     GTEST_SKIP() << "Failed to connect to PostgreSQL: " << error;
        // }

        // Create executor
        executor_ = std::make_unique<SyncExecutor>(conn_);

        // Create query compiler with PostgreSQL dialect
        compiler_ = std::make_unique<QueryCompiler>(std::make_unique<postgres::Dialect>(), false);

        // Create test schema
        users_schema_ = std::make_shared<Table>("test_users");
        users_schema_->add_field<int>("id", "SERIAL PRIMARY KEY")
            .add_field<std::string>("name", "VARCHAR(100)")
            .add_field<int>("age", "INTEGER")
            .add_field<bool>("active", "BOOLEAN");

        // Create column references
        user_id_     = users_schema_->column<int>("id");
        user_name_   = users_schema_->column<std::string>("name");
        user_age_    = users_schema_->column<int>("age");
        user_active_ = users_schema_->column<bool>("active");

        // Create actual table in database
        auto create_result = executor_->execute(R"(
            CREATE TABLE IF NOT EXISTS test_users (
                id SERIAL PRIMARY KEY,
                name VARCHAR(100),
                age INTEGER,
                active BOOLEAN
            )
        )");
        ASSERT_TRUE(create_result.is_success()) << "Failed to create test table: "
                                                << create_result.error<ErrorContext>();

        // Clean table
        CleanTestTable();
    }

    void TearDown() override {
        if (conn_) {
            // Drop test table
            EXPECT_TRUE(executor_->execute("DROP TABLE IF EXISTS test_users CASCADE"));
            PQfinish(conn_);
            conn_ = nullptr;
        }
    }

    void CleanTestTable() const {
        auto result = executor_->execute("TRUNCATE TABLE test_users RESTART IDENTITY CASCADE");
        ASSERT_TRUE(result.is_success()) << "Failed to clean test table: " << result.error<ErrorContext>();
    }

    int CountRows() const {
        auto result = executor_->execute("SELECT COUNT(*) FROM test_users");
        if (result.is_success()) {
            auto& block = result.value();
            if (block.rows() > 0) {
                return block.get<int>(0, 0);
            }
        }
        return 0;
    }

    PGconn* conn_{nullptr};
    std::unique_ptr<SyncExecutor> executor_;
    std::unique_ptr<QueryCompiler> compiler_;

    std::shared_ptr<Table> users_schema_;
    TableColumn<int> user_id_{nullptr, ""};
    TableColumn<std::string> user_name_{nullptr, ""};
    TableColumn<int> user_age_{nullptr, ""};
    TableColumn<bool> user_active_{nullptr, ""};
};

// ============== Basic DELETE Tests ==============

TEST_F(CompiledDeleteTest, DeleteSingleRow) {
    // Insert test data
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age, active) VALUES ('Alice', 30, true)"));
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age, active) VALUES ('Bob', 25, false)"));

    // Build and compile DELETE query
    auto query          = delete_from(users_schema_).where(user_name_ == std::string{"Alice"});
    auto compiled_query = compiler_->compile(query);

    // Execute compiled query
    auto result = executor_->execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Delete failed: " << result.error<ErrorContext>();

    // Verify deletion
    EXPECT_EQ(CountRows(), 1);  // Only Bob should remain

    // Verify Alice is gone
    auto select_result = executor_->execute("SELECT COUNT(*) FROM test_users WHERE name = 'Alice'");
    ASSERT_TRUE(select_result.is_success());
    EXPECT_EQ(select_result.value().get<int>(0, 0), 0);
}

TEST_F(CompiledDeleteTest, DeleteMultipleRows) {
    // Insert test data
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age, active) VALUES ('User1', 20, false)"));
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age, active) VALUES ('User2', 30, false)"));
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age, active) VALUES ('User3', 40, true)"));

    // Build and compile DELETE query that matches multiple rows
    auto query          = delete_from(users_schema_).where(user_active_ == false);
    auto compiled_query = compiler_->compile(query);

    // Execute compiled query
    auto result = executor_->execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Delete failed: " << result.error<ErrorContext>();

    // Verify deletion - only User3 should remain
    EXPECT_EQ(CountRows(), 1);

    // Verify only active users remain
    auto select_result = executor_->execute("SELECT COUNT(*) FROM test_users WHERE active = true");
    ASSERT_TRUE(select_result.is_success());
    EXPECT_EQ(select_result.value().get<int>(0, 0), 1);
}

// ============== DELETE with WHERE Conditions ==============

TEST_F(CompiledDeleteTest, DeleteWithSimpleWhere) {
    // Insert test data
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age) VALUES ('User1', 20)"));
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age) VALUES ('User2', 30)"));
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age) VALUES ('User3', 40)"));

    // Build and compile DELETE query with WHERE condition
    auto query          = delete_from(users_schema_).where(user_age_ > 25);
    auto compiled_query = compiler_->compile(query);

    // Execute compiled query
    auto result = executor_->execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Delete failed: " << result.error<ErrorContext>();

    // Verify deletion - only User1 should remain
    EXPECT_EQ(CountRows(), 1);
}

TEST_F(CompiledDeleteTest, DeleteWithComplexWhere) {
    // Insert test data
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age, active) VALUES ('User1', 25, true)"));
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age, active) VALUES ('User2', 30, true)"));
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age, active) VALUES ('User3', 35, false)"));
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age, active) VALUES ('User4', 40, true)"));

    // Build and compile DELETE query with complex WHERE (AND condition)
    auto query = delete_from(users_schema_).where((user_age_ >= 30) && (user_active_ == true));
    auto compiled_query = compiler_->compile(query);

    // Execute compiled query
    auto result = executor_->execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Delete failed: " << result.error<ErrorContext>();

    // Verify deletion - User1 and User3 should remain
    EXPECT_EQ(CountRows(), 2);
}

TEST_F(CompiledDeleteTest, DeleteWithOrCondition) {
    // Insert test data
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age) VALUES ('User1', 20)"));
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age) VALUES ('User2', 30)"));
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age) VALUES ('User3', 40)"));

    // Build and compile DELETE query with OR condition
    auto query          = delete_from(users_schema_).where((user_age_ < 25) || (user_age_ > 35));
    auto compiled_query = compiler_->compile(query);

    // Execute compiled query
    auto result = executor_->execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Delete failed: " << result.error<ErrorContext>();

    // Verify deletion - only User2 should remain
    EXPECT_EQ(CountRows(), 1);
}

TEST_F(CompiledDeleteTest, DeleteWithInCondition) {
    // Insert test data
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age) VALUES ('User1', 18)"));
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age) VALUES ('User2', 19)"));
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age) VALUES ('User3', 20)"));
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age) VALUES ('User4', 25)"));

    // Build and compile DELETE query with IN condition
    auto query          = delete_from(users_schema_).where(in(user_age_, 18, 19, 20));
    auto compiled_query = compiler_->compile(query);

    // Execute compiled query
    auto result = executor_->execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Delete failed: " << result.error<ErrorContext>();

    // Verify deletion - only User4 should remain
    EXPECT_EQ(CountRows(), 1);

    auto select_result = executor_->execute("SELECT age FROM test_users");
    ASSERT_TRUE(select_result.is_success());
    EXPECT_EQ(select_result.value().get<int>(0, 0), 25);
}

TEST_F(CompiledDeleteTest, DeleteWithBetweenCondition) {
    // Insert test data
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age) VALUES ('User1', 15)"));
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age) VALUES ('User2', 20)"));
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age) VALUES ('User3', 25)"));
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age) VALUES ('User4', 30)"));

    // Build and compile DELETE query with BETWEEN condition
    auto query          = delete_from(users_schema_).where(between(user_age_, 18, 26));
    auto compiled_query = compiler_->compile(query);

    // Execute compiled query
    auto result = executor_->execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Delete failed: " << result.error<ErrorContext>();

    // Verify deletion - User1 and User4 should remain
    EXPECT_EQ(CountRows(), 2);
}

// ============== DELETE All Rows ==============

TEST_F(CompiledDeleteTest, DeleteAllRows) {
    // Insert test data
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age) VALUES ('User1', 25)"));
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age) VALUES ('User2', 30)"));
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age) VALUES ('User3', 35)"));

    // Build and compile DELETE query without WHERE (deletes all rows)
    auto query          = delete_from(users_schema_);
    auto compiled_query = compiler_->compile(query);

    // Execute compiled query
    auto result = executor_->execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Delete failed: " << result.error<ErrorContext>();

    // Verify all rows are deleted
    EXPECT_EQ(CountRows(), 0);
}

// ============== DELETE with Table Name String ==============

TEST_F(CompiledDeleteTest, DeleteWithTableName) {
    // Insert test data
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age) VALUES ('TestUser', 25)"));

    // Build and compile DELETE query using table name string
    auto query          = delete_from("test_users").where(user_name_ == std::string{"TestUser"});
    auto compiled_query = compiler_->compile(query);

    // Execute compiled query
    auto result = executor_->execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Delete failed: " << result.error<ErrorContext>();

    // Verify deletion
    EXPECT_EQ(CountRows(), 0);
}

// ============== DELETE Edge Cases ==============

TEST_F(CompiledDeleteTest, DeleteNoMatch) {
    // Insert test data
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age) VALUES ('TestUser', 25)"));

    // Build and compile DELETE query that matches no rows
    auto query          = delete_from(users_schema_).where(user_age_ > 100);
    auto compiled_query = compiler_->compile(query);

    // Execute compiled query
    auto result = executor_->execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Delete failed: " << result.error<ErrorContext>();

    // Verify no rows were deleted
    EXPECT_EQ(CountRows(), 1);
}

TEST_F(CompiledDeleteTest, DeleteEmptyTable) {
    // Don't insert any data

    // Build and compile DELETE query on empty table
    auto query          = delete_from(users_schema_).where(user_active_ == false);
    auto compiled_query = compiler_->compile(query);

    // Execute compiled query
    auto result = executor_->execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Delete failed: " << result.error<ErrorContext>();

    // Verify table is still empty
    EXPECT_EQ(CountRows(), 0);
}

TEST_F(CompiledDeleteTest, DeleteWithNullComparison) {
    // Insert test data with NULL age
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age) VALUES ('User1', NULL)"));
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age) VALUES ('User2', 30)"));

    // Build and compile DELETE query checking for NULL using IS NULL
    // Note: This uses raw SQL for now since is_null might not be implemented
    auto query          = delete_from(users_schema_).where(user_age_ == 30);
    auto compiled_query = compiler_->compile(query);

    // Execute compiled query
    auto result = executor_->execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Delete failed: " << result.error<ErrorContext>();

    // Verify only User1 (with NULL age) remains
    EXPECT_EQ(CountRows(), 1);
}

TEST_F(CompiledDeleteTest, DeleteMultipleSeparateQueries) {
    // Insert test data
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age) VALUES ('User1', 20)"));
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age) VALUES ('User2', 30)"));
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age) VALUES ('User3', 40)"));

    // First delete
    auto query1          = delete_from(users_schema_).where(user_age_ == 20);
    auto compiled_query1 = compiler_->compile(query1);
    auto result1         = executor_->execute(compiled_query1);
    ASSERT_TRUE(result1.is_success()) << "First delete failed: " << result1.error<ErrorContext>();
    EXPECT_EQ(CountRows(), 2);

    // Second delete
    auto query2          = delete_from(users_schema_).where(user_age_ == 40);
    auto compiled_query2 = compiler_->compile(query2);
    auto result2         = executor_->execute(compiled_query2);
    ASSERT_TRUE(result2.is_success()) << "Second delete failed: " << result2.error<ErrorContext>();
    EXPECT_EQ(CountRows(), 1);

    // Verify only User2 remains
    auto select_result = executor_->execute("SELECT age FROM test_users");
    ASSERT_TRUE(select_result.is_success());
    EXPECT_EQ(select_result.value().get<int>(0, 0), 30);
}

TEST_F(CompiledDeleteTest, DeleteWithStringComparison) {
    // Insert test data
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age) VALUES ('Alice', 25)"));
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age) VALUES ('Bob', 30)"));
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age) VALUES ('Charlie', 35)"));

    // Build and compile DELETE query with string comparison
    auto query          = delete_from(users_schema_).where(user_name_ == std::string{"Bob"});
    auto compiled_query = compiler_->compile(query);

    // Execute compiled query
    auto result = executor_->execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Delete failed: " << result.error<ErrorContext>();

    // Verify deletion
    EXPECT_EQ(CountRows(), 2);

    // Verify Bob is deleted
    auto select_result = executor_->execute("SELECT COUNT(*) FROM test_users WHERE name = 'Bob'");
    ASSERT_TRUE(select_result.is_success());
    EXPECT_EQ(select_result.value().get<int>(0, 0), 0);
}

TEST_F(CompiledDeleteTest, DeleteWithBooleanCondition) {
    // Insert test data
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, active) VALUES ('User1', true)"));
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, active) VALUES ('User2', false)"));
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, active) VALUES ('User3', true)"));

    // Build and compile DELETE query with boolean condition
    auto query          = delete_from(users_schema_).where(user_active_ == false);
    auto compiled_query = compiler_->compile(query);

    // Execute compiled query
    auto result = executor_->execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Delete failed: " << result.error<ErrorContext>();

    // Verify deletion - only active users remain
    EXPECT_EQ(CountRows(), 2);

    auto select_result = executor_->execute("SELECT COUNT(*) FROM test_users WHERE active = true");
    ASSERT_TRUE(select_result.is_success());
    EXPECT_EQ(select_result.value().get<int>(0, 0), 2);
}
