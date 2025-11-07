// PostgreSQL SyncExecutor Functional Tests
// Tests the synchronous query executor with real PostgreSQL connection

#include "postgres_sync_executor.hpp"

#include <gtest/gtest.h>

#include "postgres_params.hpp"

using namespace demiplane::db::postgres;
using namespace demiplane::db;

// Test fixture for SyncExecutor
class SyncExecutorTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Get connection parameters from environment or use defaults (matching docker-compose.test.yml)
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
        if (PQstatus(conn_) != CONNECTION_OK) {
            std::string error = PQerrorMessage(conn_);
            PQfinish(conn_);
            conn_ = nullptr;
            GTEST_SKIP() << "Failed to connect to PostgreSQL: " << error
                         << "\nSet POSTGRES_HOST, POSTGRES_PORT, POSTGRES_DB, POSTGRES_USER, POSTGRES_PASSWORD "
                            "environment variables";
        }

        // Create executor
        executor_ = std::make_unique<SyncExecutor>(conn_);

        // Create test table
        auto result = executor_->execute(R"(
            CREATE TABLE IF NOT EXISTS test_users (
                id SERIAL PRIMARY KEY,
                name VARCHAR(100) NOT NULL,
                age INTEGER,
                email VARCHAR(100) UNIQUE,
                active BOOLEAN DEFAULT TRUE
            )
        )");

        ASSERT_TRUE(result.is_success()) << "Failed to create test table: " << result.error<ErrorContext>().format();

        // Clean table
        CleanTestTable();
    }

    void TearDown() override {
        if (conn_) {
            // Drop test table
            ASSERT_TRUE(executor_->execute("DROP TABLE IF EXISTS test_users CASCADE"));

            PQfinish(conn_);
            conn_ = nullptr;
        }
    }

    void CleanTestTable() const {
        auto result = executor_->execute("TRUNCATE TABLE test_users RESTART IDENTITY CASCADE");
        ASSERT_TRUE(result.is_success()) << "Failed to clean test table: " << result.error<ErrorContext>().format();
    }

    PGconn* conn_{nullptr};
    std::unique_ptr<SyncExecutor> executor_;
};

// ============== Simple Query Tests ==============

TEST_F(SyncExecutorTest, ExecuteSimpleSelect) {
    auto result = executor_->execute("SELECT 1 AS number, 'hello' AS text");

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>().format();

    auto& block = result.value();
    EXPECT_EQ(block.rows(), 1);
    EXPECT_EQ(block.cols(), 2);
}

TEST_F(SyncExecutorTest, ExecuteSimpleInsert) {
    auto result =
        executor_->execute("INSERT INTO test_users (name, age, email) VALUES ('Alice', 30, 'alice@test.com')");

    ASSERT_TRUE(result.is_success()) << "Insert failed: " << result.error<ErrorContext>().format();

    // Verify insertion
    auto select_result = executor_->execute("SELECT COUNT(*) FROM test_users");
    ASSERT_TRUE(select_result.is_success());
    EXPECT_EQ(select_result.value().rows(), 1);
}

TEST_F(SyncExecutorTest, ExecuteSimpleUpdate) {
    // Insert test data
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age) VALUES ('Bob', 25)"));
    // Update
    auto result = executor_->execute("UPDATE test_users SET age = 26 WHERE name = 'Bob'");

    ASSERT_TRUE(result.is_success()) << "Update failed: " << result.error<ErrorContext>().format();
}

TEST_F(SyncExecutorTest, ExecuteSimpleDelete) {
    // Insert test data
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age) VALUES ('Charlie', 35)"));

    // Delete
    auto result = executor_->execute("DELETE FROM test_users WHERE name = 'Charlie'");

    ASSERT_TRUE(result.is_success()) << "Delete failed: " << result.error<ErrorContext>().format();
}

TEST_F(SyncExecutorTest, ExecuteEmptyResultSet) {
    auto result = executor_->execute("SELECT * FROM test_users WHERE id = -1");

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>().format();

    auto& block = result.value();
    EXPECT_EQ(block.rows(), 0);
    EXPECT_TRUE(block.empty());
}

// ============== Parameterized Query Tests ==============

TEST_F(SyncExecutorTest, ExecuteParameterizedInsert) {
    std::pmr::unsynchronized_pool_resource pool;
    postgres::ParamSink sink(&pool);

    sink.push(FieldValue{std::string{"Dave"}});
    sink.push(FieldValue{40});
    sink.push(FieldValue{std::string{"dave@test.com"}});

    auto params = sink.native_packet();

    auto result = executor_->execute("INSERT INTO test_users (name, age, email) VALUES ($1, $2, $3)", *params);

    ASSERT_TRUE(result.is_success()) << "Parameterized insert failed: " << result.error<ErrorContext>().format();
}

TEST_F(SyncExecutorTest, ExecuteParameterizedSelect) {
    // Insert test data
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age) VALUES ('Eve', 28)"));

    // Query with parameter
    std::pmr::unsynchronized_pool_resource pool;
    postgres::ParamSink sink(&pool);
    sink.push(FieldValue{std::string{"Eve"}});
    auto params = sink.native_packet();

    auto result = executor_->execute("SELECT name, age FROM test_users WHERE name = $1", *params);

    ASSERT_TRUE(result.is_success()) << "Parameterized select failed: " << result.error<ErrorContext>().format();

    auto& block = result.value();
    EXPECT_EQ(block.rows(), 1);
}

TEST_F(SyncExecutorTest, ExecuteMultipleParameters) {
    std::pmr::unsynchronized_pool_resource pool;
    postgres::ParamSink sink(&pool);

    sink.push(FieldValue{25});
    sink.push(FieldValue{40});

    auto params = sink.native_packet();

    // Insert some test data
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age) VALUES ('User1', 20)"));
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age) VALUES ('User2', 30)"));
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age) VALUES ('User3', 45)"));

    auto result = executor_->execute("SELECT * FROM test_users WHERE age BETWEEN $1 AND $2", *params);

    ASSERT_TRUE(result.is_success()) << "Multi-parameter query failed: " << result.error<ErrorContext>().format();

    auto& block = result.value();
    EXPECT_EQ(block.rows(), 1);  // Only User2 is between 25 and 40
}

TEST_F(SyncExecutorTest, ExecuteNullParameter) {
    std::pmr::unsynchronized_pool_resource pool;
    postgres::ParamSink sink(&pool);

    sink.push(FieldValue{std::string{"NullEmailUser"}});
    sink.push(FieldValue{std::monostate{}});  // NULL value

    auto params = sink.native_packet();

    auto result = executor_->execute("INSERT INTO test_users (name, email) VALUES ($1, $2)", *params);

    ASSERT_TRUE(result.is_success()) << "Insert with NULL parameter failed: " << result.error<ErrorContext>().format();
}

// ============== Variadic Execute Tests ==============

TEST_F(SyncExecutorTest, ExecuteVariadicSingleParameter) {
    // Insert test data
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age) VALUES ('Frank', 33)"));

    // Query with single variadic parameter
    auto result = executor_->execute("SELECT name, age FROM test_users WHERE name = $1", std::string{"Frank"});

    ASSERT_TRUE(result.is_success()) << "Variadic single parameter failed: " << result.error<ErrorContext>().format();

    auto& block = result.value();
    EXPECT_EQ(block.rows(), 1);
}

TEST_F(SyncExecutorTest, ExecuteVariadicMultipleTypes) {
    // Insert with multiple types using variadic API
    auto result = executor_->execute("INSERT INTO test_users (name, age, email) VALUES ($1, $2, $3)",
                                     std::string{"Grace"},
                                     35,
                                     std::string{"grace@test.com"});

    ASSERT_TRUE(result.is_success()) << "Variadic insert failed: " << result.error<ErrorContext>().format();

    // Verify insertion
    auto select_result =
        executor_->execute("SELECT name, age FROM test_users WHERE email = $1", std::string{"grace@test.com"});
    ASSERT_TRUE(select_result.is_success());
    EXPECT_EQ(select_result.value().rows(), 1);
}

TEST_F(SyncExecutorTest, ExecuteVariadicIntegerTypes) {
    // Test with different integer types
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age) VALUES ('User1', 20)"));
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age) VALUES ('User2', 30)"));
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age) VALUES ('User3', 45)"));

    // Query with int parameters
    auto result = executor_->execute("SELECT * FROM test_users WHERE age BETWEEN $1 AND $2", 25, 40);

    ASSERT_TRUE(result.is_success()) << "Variadic int parameters failed: " << result.error<ErrorContext>().format();

    auto& block = result.value();
    EXPECT_EQ(block.rows(), 1);  // Only User2 is between 25 and 40
}

TEST_F(SyncExecutorTest, ExecuteVariadicWithNull) {
    // Insert with NULL value using std::monostate
    auto result = executor_->execute(
        "INSERT INTO test_users (name, email) VALUES ($1, $2)", std::string{"NullEmailUser2"}, std::monostate{});

    ASSERT_TRUE(result.is_success()) << "Variadic NULL parameter failed: " << result.error<ErrorContext>().format();

    // Verify NULL was inserted
    auto select_result =
        executor_->execute("SELECT name, email FROM test_users WHERE name = $1", std::string{"NullEmailUser2"});
    ASSERT_TRUE(select_result.is_success());
    auto& block = select_result.value();
    EXPECT_EQ(block.rows(), 1);
    auto email_opt = block.get_opt<std::string>(0, 1);
    EXPECT_FALSE(email_opt.has_value()) << "Email should be NULL";
}

TEST_F(SyncExecutorTest, ExecuteVariadicBooleanType) {
    // Insert with boolean value
    auto result = executor_->execute(
        "INSERT INTO test_users (name, age, active) VALUES ($1, $2, $3)", std::string{"Helen"}, 29, false);

    ASSERT_TRUE(result.is_success()) << "Variadic boolean parameter failed: " << result.error<ErrorContext>().format();

    // Verify boolean value
    auto select_result = executor_->execute("SELECT active FROM test_users WHERE name = $1", std::string{"Helen"});
    ASSERT_TRUE(select_result.is_success());
    EXPECT_EQ(select_result.value().rows(), 1);
}

TEST_F(SyncExecutorTest, ExecuteVariadicManyParameters) {
    // Test with more parameters (ensuring stack buffer is sufficient)
    auto result = executor_->execute("INSERT INTO test_users (name, age, email, active) VALUES ($1, $2, $3, $4)",
                                     std::string{"Ivan"},
                                     42,
                                     std::string{"ivan@test.com"},
                                     true);

    ASSERT_TRUE(result.is_success()) << "Variadic many parameters failed: " << result.error<ErrorContext>().format();

    // Verify with multiple query parameters
    auto select_result = executor_->execute("SELECT name FROM test_users WHERE age = $1 AND email = $2 AND active = $3",
                                            42,
                                            std::string{"ivan@test.com"},
                                            true);

    ASSERT_TRUE(select_result.is_success());
    EXPECT_EQ(select_result.value().rows(), 1);
}

TEST_F(SyncExecutorTest, ExecuteVariadicComplexQuery) {
    // Insert test data
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age, active) VALUES ('ActiveUser1', 25, true)"));
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age, active) VALUES ('ActiveUser2', 30, true)"));
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age, active) VALUES ('InactiveUser', 35, false)"));

    // Complex query with variadic parameters
    auto result = executor_->execute(
        "SELECT name, age FROM test_users WHERE age >= $1 AND age <= $2 AND active = $3 ORDER BY age", 20, 35, true);

    ASSERT_TRUE(result.is_success()) << "Variadic complex query failed: " << result.error<ErrorContext>().format();

    auto& block = result.value();
    EXPECT_EQ(block.rows(), 2);  // ActiveUser1 and ActiveUser2
}

// ============== Error Handling Tests ==============

TEST_F(SyncExecutorTest, SyntaxError) {
    auto result = executor_->execute("SELCT * FROM test_users");  // Typo: SELCT

    ASSERT_FALSE(result.is_success()) << "Should have failed with syntax error";

    auto& error = result.error<ErrorContext>();
    EXPECT_FALSE(error.sqlstate.empty());
    EXPECT_EQ(error.sqlstate.substr(0, 2), "42");  // Class 42 = Syntax Error or Access Rule Violation
    EXPECT_FALSE(error.message.empty());
}

TEST_F(SyncExecutorTest, UniqueConstraintViolation) {
    // Insert first user
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, email) VALUES ('User1', 'duplicate@test.com')"));

    // Try to insert duplicate email
    auto result = executor_->execute("INSERT INTO test_users (name, email) VALUES ('User2', 'duplicate@test.com')");

    ASSERT_FALSE(result.is_success()) << "Should have failed with unique constraint violation";

    auto& error = result.error<ErrorContext>();
    EXPECT_EQ(error.sqlstate, "23505");  // Unique violation
    EXPECT_TRUE(error.code.is_server_error());
    EXPECT_EQ(error.code, ServerErrorCode::UniqueViolation);
}

TEST_F(SyncExecutorTest, NotNullConstraintViolation) {
    auto result = executor_->execute("INSERT INTO test_users (age) VALUES (25)");  // name is NOT NULL

    ASSERT_FALSE(result.is_success()) << "Should have failed with NOT NULL constraint violation";

    auto& error = result.error<ErrorContext>();
    EXPECT_EQ(error.sqlstate, "23502");  // NOT NULL violation
    EXPECT_EQ(error.code, ServerErrorCode::NotNullViolation);
}

TEST_F(SyncExecutorTest, TableNotFound) {
    auto result = executor_->execute("SELECT * FROM non_existent_table");

    ASSERT_FALSE(result.is_success()) << "Should have failed with table not found error";

    auto& error = result.error<ErrorContext>();
    EXPECT_EQ(error.sqlstate, "42P01");  // Undefined table
    EXPECT_EQ(error.code, ServerErrorCode::TableNotFound);
}

TEST_F(SyncExecutorTest, InvalidConnectionError) {
    // Create executor with null connection
    SyncExecutor invalid_executor(nullptr);

    auto result = invalid_executor.execute("SELECT 1");

    ASSERT_FALSE(result.is_success()) << "Should have failed with connection error";

    auto& error = result.error<ErrorContext>();
    EXPECT_TRUE(error.code.is_client_error());
    EXPECT_EQ(error.code, ClientErrorCode::NotConnected);
}

// ============== Result Processing Tests ==============

TEST_F(SyncExecutorTest, MultipleRowsResult) {
    // Insert multiple rows
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age) VALUES ('User1', 21)"));
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age) VALUES ('User2', 22)"));
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age) VALUES ('User3', 23)"));

    auto result = executor_->execute("SELECT name, age FROM test_users ORDER BY age");

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>().format();

    auto& block = result.value();
    EXPECT_EQ(block.rows(), 3);
    EXPECT_EQ(block.cols(), 2);
}

TEST_F(SyncExecutorTest, NullValuesInResult) {
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age) VALUES ('NullAge', NULL)"));

    auto result = executor_->execute("SELECT name, age FROM test_users WHERE name = 'NullAge'");

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>().format();

    auto& block = result.value();
    EXPECT_EQ(block.rows(), 1);

    // Check NULL value handling
    auto age_opt = block.get_opt<int>(0, 1);
    EXPECT_FALSE(age_opt.has_value()) << "Age should be NULL";
}

// ============== Edge Cases ==============

TEST_F(SyncExecutorTest, EmptyQuery) {
    auto result = executor_->execute("");

    // Empty query should result in error
    ASSERT_FALSE(result.is_success()) << "Empty query should fail";
    auto& error = result.error<ErrorContext>();

    // PGRES_EMPTY_QUERY doesn't provide SQLSTATE, falls back to status-based mapping
    EXPECT_TRUE(error.sqlstate.empty()) << "Empty query has no SQLSTATE";
    EXPECT_EQ(error.code, ClientErrorCode::InvalidArgument);
    EXPECT_TRUE(error.code.is_client_error());
}

TEST_F(SyncExecutorTest, LargeResultSet) {
    // Insert 1000 rows
    for (int i = 0; i < 1000; ++i) {
        std::string query = "INSERT INTO test_users (name, age) VALUES ('User" + std::to_string(i) + "', " +
                            std::to_string(20 + i % 50) + ")";
        auto result = executor_->execute(query);
        ASSERT_TRUE(result.is_success()) << "Insert failed at iteration " << i;
    }

    auto result = executor_->execute("SELECT * FROM test_users");

    ASSERT_TRUE(result.is_success()) << "Large query failed: " << result.error<ErrorContext>().format();

    auto& block = result.value();
    EXPECT_EQ(block.rows(), 1000);
}
