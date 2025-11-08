// Compiled SELECT Query Functional Tests
// Tests query compilation + execution with SyncExecutor

#include "postgres_sync_executor.hpp"

#include <gtest/gtest.h>

#include "postgres_dialect.hpp"
#include "postgres_params.hpp"
#include "query_compiler.hpp"

using namespace demiplane::db;
using namespace demiplane::db::postgres;

// Test fixture for compiled SELECT queries
class CompiledSelectTest : public ::testing::Test {
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

        // TODO: CE bcuz of try of B expression postgres
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

    PGconn* conn_{nullptr};
    std::unique_ptr<SyncExecutor> executor_;
    std::unique_ptr<QueryCompiler> compiler_;

    std::shared_ptr<Table> users_schema_;
    TableColumn<int> user_id_{nullptr, ""};
    TableColumn<std::string> user_name_{nullptr, ""};
    TableColumn<int> user_age_{nullptr, ""};
    TableColumn<bool> user_active_{nullptr, ""};
};

// ============== Basic SELECT Tests ==============

TEST_F(CompiledSelectTest, SelectAllColumns) {
    // Insert test data
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age, active) VALUES ('Alice', 30, true)"));
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age, active) VALUES ('Bob', 25, false)"));

    // Build and compile query
    auto query          = select(all("test_users")).from(users_schema_);
    auto compiled_query = compiler_->compile(query);

    // Execute compiled query
    auto result = executor_->execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_EQ(block.rows(), 2);
}

TEST_F(CompiledSelectTest, SelectSpecificColumns) {
    // Insert test data
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age) VALUES ('Charlie', 35)"));

    // Build and compile query - select only name and age
    auto query          = select(user_name_, user_age_).from(users_schema_);
    auto compiled_query = compiler_->compile(query);

    // Execute compiled query
    auto result = executor_->execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_EQ(block.rows(), 1);
    EXPECT_EQ(block.cols(), 2);
}

TEST_F(CompiledSelectTest, SelectWithWhere) {
    // Insert test data
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age, active) VALUES ('Dave', 20, true)"));
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age, active) VALUES ('Eve', 30, true)"));
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age, active) VALUES ('Frank', 40, false)"));

    // Build and compile query with WHERE clause
    auto query          = select(user_name_, user_age_).from(users_schema_).where(user_age_ > 25);
    auto compiled_query = compiler_->compile(query);

    // Execute compiled query
    auto result = executor_->execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_EQ(block.rows(), 2);  // Eve and Frank
}

TEST_F(CompiledSelectTest, SelectWithComplexWhere) {
    // Insert test data
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age, active) VALUES ('User1', 25, true)"));
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age, active) VALUES ('User2', 30, true)"));
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age, active) VALUES ('User3', 35, false)"));

    // Build and compile query with complex WHERE (AND condition)
    auto query = select(user_name_).from(users_schema_).where((user_age_ >= 25) && (user_active_ == true));
    auto compiled_query = compiler_->compile(query);

    // Execute compiled query
    auto result = executor_->execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_EQ(block.rows(), 2);  // User1 and User2
}

// ============== SELECT with ORDER BY ==============

TEST_F(CompiledSelectTest, SelectWithOrderBy) {
    // Insert test data in random order
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age) VALUES ('Zebra', 30)"));
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age) VALUES ('Alpha', 25)"));
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age) VALUES ('Beta', 35)"));

    // Build and compile query with ORDER BY
    auto query          = select(user_name_, user_age_).from(users_schema_).order_by(asc(user_name_));
    auto compiled_query = compiler_->compile(query);

    // Execute compiled query
    auto result = executor_->execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_EQ(block.rows(), 3);
    // Results should be ordered: Alpha, Beta, Zebra
}

TEST_F(CompiledSelectTest, SelectWithOrderByDesc) {
    // Insert test data
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age) VALUES ('User1', 20)"));
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age) VALUES ('User2', 30)"));
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age) VALUES ('User3', 25)"));

    // Build and compile query with DESC ORDER BY
    auto query          = select(user_name_, user_age_).from(users_schema_).order_by(desc(user_age_));
    auto compiled_query = compiler_->compile(query);

    // Execute compiled query
    auto result = executor_->execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_EQ(block.rows(), 3);
    // Results should be ordered by age DESC: 30, 25, 20
}

// ============== SELECT with LIMIT ==============

TEST_F(CompiledSelectTest, SelectWithLimit) {
    // Insert test data
    for (int i = 1; i <= 10; ++i) {
        EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age) VALUES ('User" + std::to_string(i) +
                                       "', " + std::to_string(20 + i) + ")"));
    }

    // Build and compile query with LIMIT
    auto query          = select(user_name_).from(users_schema_).limit(5);
    auto compiled_query = compiler_->compile(query);

    // Execute compiled query
    auto result = executor_->execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_EQ(block.rows(), 5);
}

TEST_F(CompiledSelectTest, SelectWithLimitAndOffset) {
    // Insert test data
    for (int i = 1; i <= 10; ++i) {
        EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age) VALUES ('User" + std::to_string(i) +
                                       "', " + std::to_string(20 + i) + ")"));
    }

    // Build and compile query with LIMIT and OFFSET
    auto query          = select(user_name_).from(users_schema_).limit(3).offset(2);
    auto compiled_query = compiler_->compile(query);

    // Execute compiled query
    auto result = executor_->execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_EQ(block.rows(), 3);  // Skip 2, return 3
}

// ============== SELECT DISTINCT ==============

TEST_F(CompiledSelectTest, SelectDistinct) {
    // Insert duplicate data
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age) VALUES ('Alice', 30)"));
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age) VALUES ('Alice', 30)"));
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age) VALUES ('Bob', 25)"));

    // Build and compile DISTINCT query
    auto query          = select_distinct(user_name_, user_age_).from(users_schema_);
    auto compiled_query = compiler_->compile(query);

    // Execute compiled query
    auto result = executor_->execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_EQ(block.rows(), 2);  // Only 2 distinct rows
}

// ============== SELECT with Aggregates ==============

TEST_F(CompiledSelectTest, SelectWithCount) {
    // Insert test data
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age) VALUES ('User1', 25)"));
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age) VALUES ('User2', 30)"));
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age) VALUES ('User3', 35)"));

    // Build and compile COUNT query
    auto query          = select(count(user_id_).as("total")).from(users_schema_);
    auto compiled_query = compiler_->compile(query);

    // Execute compiled query
    auto result = executor_->execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_EQ(block.rows(), 1);
    EXPECT_EQ(block.cols(), 1);
}

TEST_F(CompiledSelectTest, SelectWithGroupBy) {
    // Insert test data with different active status
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age, active) VALUES ('User1', 25, true)"));
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age, active) VALUES ('User2', 30, true)"));
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age, active) VALUES ('User3', 35, false)"));

    // Build and compile query with GROUP BY
    auto query = select(user_active_, count(user_id_).as("count")).from(users_schema_).group_by(user_active_);
    auto compiled_query = compiler_->compile(query);

    // Execute compiled query
    auto result = executor_->execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_EQ(block.rows(), 2);  // Two groups: true and false
}

TEST_F(CompiledSelectTest, SelectWithHaving) {
    // Insert test data
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age, active) VALUES ('User1', 25, true)"));
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age, active) VALUES ('User2', 30, true)"));
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age, active) VALUES ('User3', 35, false)"));

    // Build and compile query with HAVING
    auto query = select(user_active_, count(user_id_).as("count"))
                     .from(users_schema_)
                     .group_by(user_active_)
                     .having(count(user_id_) > 1);
    auto compiled_query = compiler_->compile(query);

    // Execute compiled query
    auto result = executor_->execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_EQ(block.rows(), 1);  // Only the group with count > 1
}

// ============== Empty Result Tests ==============

TEST_F(CompiledSelectTest, SelectEmptyResult) {
    // Don't insert any data

    // Build and compile query
    auto query          = select(user_name_).from(users_schema_);
    auto compiled_query = compiler_->compile(query);

    // Execute compiled query
    auto result = executor_->execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_EQ(block.rows(), 0);
    EXPECT_TRUE(block.empty());
}

TEST_F(CompiledSelectTest, SelectWithNoMatch) {
    // Insert test data
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age) VALUES ('Alice', 25)"));

    // Build and compile query that won't match
    auto query          = select(user_name_).from(users_schema_).where(user_age_ > 100);
    auto compiled_query = compiler_->compile(query);

    // Execute compiled query
    auto result = executor_->execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_EQ(block.rows(), 0);
}
