// PostgreSQL AsyncExecutor Functional Tests
// Tests the asynchronous query executor with real PostgreSQL connection

#include "postgres_async_executor.hpp"

#include <boost/asio.hpp>
#include <gtest/gtest.h>

#include "postgres_params.hpp"

using namespace demiplane::db::postgres;
using namespace demiplane::db;

// Test fixture for AsyncExecutor
class AsyncExecutorTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Get connection parameters from environment or use defaults (matching docker-compose.test.yml)
        const char* host     = std::getenv("POSTGRES_HOST") ? std::getenv("POSTGRES_HOST") : "localhost";
        const char* port     = std::getenv("POSTGRES_PORT") ? std::getenv("POSTGRES_PORT") : "5433";
        const char* dbname   = std::getenv("POSTGRES_DB") ? std::getenv("POSTGRES_DB") : "test_db";
        const char* user     = std::getenv("POSTGRES_USER") ? std::getenv("POSTGRES_USER") : "test_user";
        const char* password = std::getenv("POSTGRES_PASSWORD") ? std::getenv("POSTGRES_PASSWORD") : "test_password";

        // Create connection string
        const std::string conn_info = "host=" + std::string(host) + " port=" + std::string(port) +
                                      " dbname=" + std::string(dbname) + " user=" + std::string(user) +
                                      " password=" + std::string(password);

        // Connect to database
        conn_ = PQconnectdb(conn_info.c_str());

        // Check connection status
        if (PQstatus(conn_) != CONNECTION_OK) {
            const std::string error = PQerrorMessage(conn_);
            PQfinish(conn_);
            conn_ = nullptr;
            GTEST_SKIP() << "Failed to connect to PostgreSQL: " << error
                         << "\nSet POSTGRES_HOST, POSTGRES_PORT, POSTGRES_DB, POSTGRES_USER, POSTGRES_PASSWORD "
                            "environment variables";
        }

        // Create executor
        executor_ = std::make_unique<AsyncExecutor>(conn_, io_context_.get_executor());

        // Create test table (using sync operation for setup)
        CreateTestTableSync();

        // Clean table
        CleanTestTableSync();
    }

    void TearDown() override {
        if (conn_) {
            // Drop test table (using sync operation for teardown)
            DropTestTableSync();

            PQfinish(conn_);
            conn_ = nullptr;
        }
    }

    void CreateTestTableSync() const {
        PGresult* result = PQexec(conn_, R"(
            CREATE TABLE IF NOT EXISTS test_users (
                id SERIAL PRIMARY KEY,
                name VARCHAR(100) NOT NULL,
                age INTEGER,
                email VARCHAR(100) UNIQUE,
                active BOOLEAN DEFAULT TRUE
            )
        )");

        const ExecStatusType status = PQresultStatus(result);
        PQclear(result);
        ASSERT_TRUE(status == PGRES_COMMAND_OK || status == PGRES_TUPLES_OK) << "Failed to create test table";
    }

    void CleanTestTableSync() const {
        PGresult* result            = PQexec(conn_, "TRUNCATE TABLE test_users RESTART IDENTITY CASCADE");
        const ExecStatusType status = PQresultStatus(result);
        PQclear(result);
        ASSERT_TRUE(status == PGRES_COMMAND_OK) << "Failed to clean test table";
    }

    void DropTestTableSync() const {
        PGresult* result = PQexec(conn_, "DROP TABLE IF EXISTS test_users CASCADE");
        PQclear(result);
    }

    // Helper to run async operations synchronously in tests
    template <typename CoroFunc>
    auto run_async(CoroFunc&& func) {
        using awaitable_type = std::invoke_result_t<CoroFunc>;
        using result_type    = awaitable_type::value_type;

        std::optional<result_type> result;
        std::exception_ptr eptr;

        boost::asio::co_spawn(
            io_context_,
            [&]() -> boost::asio::awaitable<void> {
                try {
                    result = co_await func();
                } catch (...) {
                    eptr = std::current_exception();
                }
            },
            boost::asio::detached);

        io_context_.run();
        io_context_.restart();

        if (eptr) {
            std::rethrow_exception(eptr);
        }

        return result;
    }

    boost::asio::io_context io_context_;
    PGconn* conn_{nullptr};
    std::unique_ptr<AsyncExecutor> executor_;
};

// ============== Simple Query Tests ==============

TEST_F(AsyncExecutorTest, ExecuteSimpleSelect) {
    auto result = run_async([this]() { return executor_->execute("SELECT 1 AS number, 'hello' AS text"); });

    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->is_success()) << "Query failed: " << result->error<ErrorContext>().format();

    const auto& block = result->value();
    EXPECT_EQ(block.rows(), 1);
    EXPECT_EQ(block.cols(), 2);
}

TEST_F(AsyncExecutorTest, ExecuteSimpleInsert) {
    auto result = run_async([this]() {
        return executor_->execute("INSERT INTO test_users (name, age, email) VALUES ('Alice', 30, 'alice@test.com')");
    });

    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->is_success()) << "Insert failed: " << result->error<ErrorContext>().format();

    // Verify insertion
    auto select_result = run_async([this]() { return executor_->execute("SELECT COUNT(*) FROM test_users"); });
    ASSERT_TRUE(select_result.has_value());
    ASSERT_TRUE(select_result->is_success());
    EXPECT_EQ(select_result->value().rows(), 1);
}

TEST_F(AsyncExecutorTest, ExecuteSimpleUpdate) {
    // Insert test data
    auto insert_result =
        run_async([this]() { return executor_->execute("INSERT INTO test_users (name, age) VALUES ('Bob', 25)"); });
    EXPECT_TRUE(insert_result.has_value() && insert_result->is_success());

    // Update
    auto result =
        run_async([this]() { return executor_->execute("UPDATE test_users SET age = 26 WHERE name = 'Bob'"); });

    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->is_success()) << "Update failed: " << result->error<ErrorContext>().format();
}

TEST_F(AsyncExecutorTest, ExecuteSimpleDelete) {
    // Insert test data
    auto insert_result =
        run_async([this]() { return executor_->execute("INSERT INTO test_users (name, age) VALUES ('Charlie', 35)"); });
    EXPECT_TRUE(insert_result.has_value() && insert_result->is_success());

    // Delete
    auto result = run_async([this]() { return executor_->execute("DELETE FROM test_users WHERE name = 'Charlie'"); });

    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->is_success()) << "Delete failed: " << result->error<ErrorContext>().format();
}

TEST_F(AsyncExecutorTest, ExecuteEmptyResultSet) {
    auto result = run_async([this]() { return executor_->execute("SELECT * FROM test_users WHERE id = -1"); });

    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->is_success()) << "Query failed: " << result->error<ErrorContext>().format();

    const auto& block = result->value();
    EXPECT_EQ(block.rows(), 0);
    EXPECT_TRUE(block.empty());
}

// ============== Parameterized Query Tests ==============

TEST_F(AsyncExecutorTest, ExecuteParameterizedInsert) {
    std::pmr::unsynchronized_pool_resource pool;
    postgres::ParamSink sink(&pool);

    sink.push(FieldValue{std::string{"Dave"}});
    sink.push(FieldValue{40});
    sink.push(FieldValue{std::string{"dave@test.com"}});

    const auto params = sink.native_packet();

    auto result = run_async([this, &params]() {
        return executor_->execute("INSERT INTO test_users (name, age, email) VALUES ($1, $2, $3)", *params);
    });

    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->is_success()) << "Parameterized insert failed: " << result->error<ErrorContext>().format();
}

TEST_F(AsyncExecutorTest, ExecuteParameterizedSelect) {
    // Insert test data
    auto insert_result =
        run_async([this]() { return executor_->execute("INSERT INTO test_users (name, age) VALUES ('Eve', 28)"); });
    EXPECT_TRUE(insert_result.has_value() && insert_result->is_success());

    // Query with parameter
    std::pmr::unsynchronized_pool_resource pool;
    postgres::ParamSink sink(&pool);
    sink.push(FieldValue{std::string{"Eve"}});
    const auto params = sink.native_packet();

    auto result = run_async(
        [this, &params]() { return executor_->execute("SELECT name, age FROM test_users WHERE name = $1", *params); });

    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->is_success()) << "Parameterized select failed: " << result->error<ErrorContext>().format();

    const auto& block = result->value();
    EXPECT_EQ(block.rows(), 1);
}

TEST_F(AsyncExecutorTest, ExecuteMultipleParameters) {
    std::pmr::unsynchronized_pool_resource pool;
    postgres::ParamSink sink(&pool);

    sink.push(FieldValue{25});
    sink.push(FieldValue{40});

    auto params = sink.native_packet();

    // Insert some test data
    run_async([this]() { return executor_->execute("INSERT INTO test_users (name, age) VALUES ('User1', 20)"); });
    run_async([this]() { return executor_->execute("INSERT INTO test_users (name, age) VALUES ('User2', 30)"); });
    run_async([this]() { return executor_->execute("INSERT INTO test_users (name, age) VALUES ('User3', 45)"); });

    auto result = run_async([this, &params]() {
        return executor_->execute("SELECT * FROM test_users WHERE age BETWEEN $1 AND $2", *params);
    });

    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->is_success()) << "Multi-parameter query failed: " << result->error<ErrorContext>().format();

    auto& block = result->value();
    EXPECT_EQ(block.rows(), 1);  // Only User2 is between 25 and 40
}

TEST_F(AsyncExecutorTest, ExecuteNullParameter) {
    std::pmr::unsynchronized_pool_resource pool;
    postgres::ParamSink sink(&pool);

    sink.push(FieldValue{std::string{"NullEmailUser"}});
    sink.push(FieldValue{std::monostate{}});  // NULL value

    const auto params = sink.native_packet();

    auto result = run_async([this, &params]() {
        return executor_->execute("INSERT INTO test_users (name, email) VALUES ($1, $2)", *params);
    });

    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->is_success()) << "Insert with NULL parameter failed: "
                                      << result->error<ErrorContext>().format();
}

// ============== Variadic Execute Tests ==============

TEST_F(AsyncExecutorTest, ExecuteVariadicSingleParameter) {
    // Insert test data
    auto insert_result =
        run_async([this]() { return executor_->execute("INSERT INTO test_users (name, age) VALUES ('Frank', 33)"); });
    EXPECT_TRUE(insert_result.has_value() && insert_result->is_success());

    // Query with single variadic parameter
    auto result = run_async([this]() {
        return executor_->execute("SELECT name, age FROM test_users WHERE name = $1", std::string{"Frank"});
    });

    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->is_success()) << "Variadic single parameter failed: " << result->error<ErrorContext>().format();

    const auto& block = result->value();
    EXPECT_EQ(block.rows(), 1);
}

TEST_F(AsyncExecutorTest, ExecuteVariadicMultipleTypes) {
    // Insert with multiple types using variadic API
    auto result = run_async([this]() {
        return executor_->execute("INSERT INTO test_users (name, age, email) VALUES ($1, $2, $3)",
                                  std::string{"Grace"},
                                  35,
                                  std::string{"grace@test.com"});
    });

    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->is_success()) << "Variadic insert failed: " << result->error<ErrorContext>().format();

    // Verify insertion
    auto select_result = run_async([this]() {
        return executor_->execute("SELECT name, age FROM test_users WHERE email = $1", std::string{"grace@test.com"});
    });
    ASSERT_TRUE(select_result.has_value());
    ASSERT_TRUE(select_result->is_success());
    EXPECT_EQ(select_result->value().rows(), 1);
}

TEST_F(AsyncExecutorTest, ExecuteVariadicIntegerTypes) {
    // Test with different integer types
    run_async([this]() { return executor_->execute("INSERT INTO test_users (name, age) VALUES ('User1', 20)"); });
    run_async([this]() { return executor_->execute("INSERT INTO test_users (name, age) VALUES ('User2', 30)"); });
    run_async([this]() { return executor_->execute("INSERT INTO test_users (name, age) VALUES ('User3', 45)"); });

    // Query with int parameters
    auto result = run_async(
        [this]() { return executor_->execute("SELECT * FROM test_users WHERE age BETWEEN $1 AND $2", 25, 40); });

    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->is_success()) << "Variadic int parameters failed: " << result->error<ErrorContext>().format();

    const auto& block = result->value();
    EXPECT_EQ(block.rows(), 1);  // Only User2 is between 25 and 40
}

TEST_F(AsyncExecutorTest, ExecuteVariadicWithNull) {
    // Insert with NULL value using std::monostate
    auto result = run_async([this]() {
        return executor_->execute(
            "INSERT INTO test_users (name, email) VALUES ($1, $2)", std::string{"NullEmailUser2"}, std::monostate{});
    });

    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->is_success()) << "Variadic NULL parameter failed: " << result->error<ErrorContext>().format();

    // Verify NULL was inserted
    auto select_result = run_async([this]() {
        return executor_->execute("SELECT name, email FROM test_users WHERE name = $1", std::string{"NullEmailUser2"});
    });
    ASSERT_TRUE(select_result.has_value());
    ASSERT_TRUE(select_result->is_success());
    auto& block = select_result->value();
    EXPECT_EQ(block.rows(), 1);
    auto email_opt = block.get_opt<std::string>(0, 1);
    EXPECT_FALSE(email_opt.has_value()) << "Email should be NULL";
}

TEST_F(AsyncExecutorTest, ExecuteVariadicBooleanType) {
    // Insert with boolean value
    auto result = run_async([this]() {
        return executor_->execute(
            "INSERT INTO test_users (name, age, active) VALUES ($1, $2, $3)", std::string{"Helen"}, 29, false);
    });

    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->is_success()) << "Variadic boolean parameter failed: "
                                      << result->error<ErrorContext>().format();

    // Verify boolean value
    auto select_result = run_async(
        [this]() { return executor_->execute("SELECT active FROM test_users WHERE name = $1", std::string{"Helen"}); });
    ASSERT_TRUE(select_result.has_value());
    ASSERT_TRUE(select_result->is_success());
    EXPECT_EQ(select_result->value().rows(), 1);
}

TEST_F(AsyncExecutorTest, ExecuteVariadicManyParameters) {
    // Test with more parameters (ensuring stack buffer is sufficient)
    auto result = run_async([this]() {
        return executor_->execute("INSERT INTO test_users (name, age, email, active) VALUES ($1, $2, $3, $4)",
                                  std::string{"Ivan"},
                                  42,
                                  std::string{"ivan@test.com"},
                                  true);
    });

    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->is_success()) << "Variadic many parameters failed: " << result->error<ErrorContext>().format();

    // Verify with multiple query parameters
    auto select_result = run_async([this]() {
        return executor_->execute("SELECT name FROM test_users WHERE age = $1 AND email = $2 AND active = $3",
                                  42,
                                  std::string{"ivan@test.com"},
                                  true);
    });

    ASSERT_TRUE(select_result.has_value());
    ASSERT_TRUE(select_result->is_success());
    EXPECT_EQ(select_result->value().rows(), 1);
}

TEST_F(AsyncExecutorTest, ExecuteVariadicComplexQuery) {
    // Insert test data
    run_async([this]() {
        return executor_->execute("INSERT INTO test_users (name, age, active) VALUES ('ActiveUser1', 25, true)");
    });
    run_async([this]() {
        return executor_->execute("INSERT INTO test_users (name, age, active) VALUES ('ActiveUser2', 30, true)");
    });
    run_async([this]() {
        return executor_->execute("INSERT INTO test_users (name, age, active) VALUES ('InactiveUser', 35, false)");
    });

    // Complex query with variadic parameters
    auto result = run_async([this]() {
        return executor_->execute(
            "SELECT name, age FROM test_users WHERE age >= $1 AND age <= $2 AND active = $3 ORDER BY age",
            20,
            35,
            true);
    });

    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->is_success()) << "Variadic complex query failed: " << result->error<ErrorContext>().format();

    const auto& block = result->value();
    EXPECT_EQ(block.rows(), 2);  // ActiveUser1 and ActiveUser2
}

// ============== Error Handling Tests ==============

TEST_F(AsyncExecutorTest, SyntaxError) {
    auto result = run_async([this]() { return executor_->execute("SELCT * FROM test_users"); });  // Typo: SELCT

    ASSERT_TRUE(result.has_value());
    ASSERT_FALSE(result->is_success()) << "Should have failed with syntax error";

    const auto& error = result->error<ErrorContext>();
    EXPECT_FALSE(error.sqlstate.empty());
    EXPECT_EQ(error.sqlstate.substr(0, 2), "42");  // Class 42 = Syntax Error or Access Rule Violation
    EXPECT_FALSE(error.message.empty());
}

TEST_F(AsyncExecutorTest, UniqueConstraintViolation) {
    // Insert first user
    auto insert_result = run_async([this]() {
        return executor_->execute("INSERT INTO test_users (name, email) VALUES ('User1', 'duplicate@test.com')");
    });
    EXPECT_TRUE(insert_result.has_value() && insert_result->is_success());

    // Try to insert duplicate email
    auto result = run_async([this]() {
        return executor_->execute("INSERT INTO test_users (name, email) VALUES ('User2', 'duplicate@test.com')");
    });

    ASSERT_TRUE(result.has_value());
    ASSERT_FALSE(result->is_success()) << "Should have failed with unique constraint violation";

    const auto& error = result->error<ErrorContext>();
    EXPECT_EQ(error.sqlstate, "23505");  // Unique violation
    EXPECT_TRUE(error.code.is_server_error());
    EXPECT_EQ(error.code, ServerErrorCode::UniqueViolation);
}

TEST_F(AsyncExecutorTest, NotNullConstraintViolation) {
    auto result = run_async(
        [this]() { return executor_->execute("INSERT INTO test_users (age) VALUES (25)"); });  // name is NOT NULL

    ASSERT_TRUE(result.has_value());
    ASSERT_FALSE(result->is_success()) << "Should have failed with NOT NULL constraint violation";

    const auto& error = result->error<ErrorContext>();
    EXPECT_EQ(error.sqlstate, "23502");  // NOT NULL violation
    EXPECT_EQ(error.code, ServerErrorCode::NotNullViolation);
}

TEST_F(AsyncExecutorTest, TableNotFound) {
    auto result = run_async([this]() { return executor_->execute("SELECT * FROM non_existent_table"); });

    ASSERT_TRUE(result.has_value());
    ASSERT_FALSE(result->is_success()) << "Should have failed with table not found error";

    const auto& error = result->error<ErrorContext>();
    EXPECT_EQ(error.sqlstate, "42P01");  // Undefined table
    EXPECT_EQ(error.code, ServerErrorCode::TableNotFound);
}

TEST_F(AsyncExecutorTest, InvalidConnectionError) {
    // Create executor with null connection (should throw in constructor)
    EXPECT_THROW({ AsyncExecutor invalid_executor(nullptr, io_context_.get_executor()); }, std::runtime_error);
}

// ============== Result Processing Tests ==============

TEST_F(AsyncExecutorTest, MultipleRowsResult) {
    // Insert multiple rows
    run_async([this]() { return executor_->execute("INSERT INTO test_users (name, age) VALUES ('User1', 21)"); });
    run_async([this]() { return executor_->execute("INSERT INTO test_users (name, age) VALUES ('User2', 22)"); });
    run_async([this]() { return executor_->execute("INSERT INTO test_users (name, age) VALUES ('User3', 23)"); });

    auto result = run_async([this]() { return executor_->execute("SELECT name, age FROM test_users ORDER BY age"); });

    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->is_success()) << "Query failed: " << result->error<ErrorContext>().format();

    const auto& block = result->value();
    EXPECT_EQ(block.rows(), 3);
    EXPECT_EQ(block.cols(), 2);
}

TEST_F(AsyncExecutorTest, NullValuesInResult) {
    auto insert_result = run_async(
        [this]() { return executor_->execute("INSERT INTO test_users (name, age) VALUES ('NullAge', NULL)"); });
    EXPECT_TRUE(insert_result.has_value() && insert_result->is_success());

    auto result =
        run_async([this]() { return executor_->execute("SELECT name, age FROM test_users WHERE name = 'NullAge'"); });

    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->is_success()) << "Query failed: " << result->error<ErrorContext>().format();

    const auto& block = result->value();
    EXPECT_EQ(block.rows(), 1);

    // Check NULL value handling
    const auto age_opt = block.get_opt<int>(0, 1);
    EXPECT_FALSE(age_opt.has_value()) << "Age should be NULL";
}

// ============== Edge Cases ==============

TEST_F(AsyncExecutorTest, EmptyQuery) {
    auto result = run_async([this]() { return executor_->execute(""); });

    ASSERT_TRUE(result.has_value());
    // Empty query should result in error
    ASSERT_FALSE(result->is_success()) << "Empty query should fail";
    const auto& error = result->error<ErrorContext>();

    // PGRES_EMPTY_QUERY doesn't provide SQLSTATE, falls back to status-based mapping
    EXPECT_TRUE(error.sqlstate.empty()) << "Empty query has no SQLSTATE";
    EXPECT_EQ(error.code, ClientErrorCode::InvalidArgument);
    EXPECT_TRUE(error.code.is_client_error());
}

TEST_F(AsyncExecutorTest, LargeResultSet) {
    // Insert 100 rows (smaller than sync test for faster execution)
    for (int i = 0; i < 100; ++i) {
        std::string query = "INSERT INTO test_users (name, age) VALUES ('User" + std::to_string(i) + "', " +
                            std::to_string(20 + i % 50) + ")";
        auto result = run_async([this, &query]() { return executor_->execute(query); });
        ASSERT_TRUE(result.has_value() && result->is_success()) << "Insert failed at iteration " << i;
    }

    auto result = run_async([this]() { return executor_->execute("SELECT * FROM test_users"); });

    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->is_success()) << "Large query failed: " << result->error<ErrorContext>().format();

    auto& block = result->value();
    EXPECT_EQ(block.rows(), 100);
}

// ============== Executor State Tests ==============

TEST_F(AsyncExecutorTest, ExecutorAccessors) {
    EXPECT_TRUE(executor_->valid());
    EXPECT_NE(executor_->native_handle(), nullptr);
    EXPECT_EQ(executor_->native_handle(), conn_);
}

TEST_F(AsyncExecutorTest, ExecutorMoveSemantics) {
    // executor_ already exists from SetUp()
    EXPECT_TRUE(executor_->valid());
    PGconn* original_conn = executor_->native_handle();

    // Move construct from existing executor
    AsyncExecutor executor2{std::move(*executor_)};
    EXPECT_TRUE(executor2.valid());
    EXPECT_FALSE(executor_->valid());  // Moved-from should be invalid
    EXPECT_EQ(executor2.native_handle(), original_conn);

    // Move assign
    AsyncExecutor executor3{std::move(executor2)};
    EXPECT_TRUE(executor3.valid());
    EXPECT_FALSE(executor2.valid());
    EXPECT_EQ(executor3.native_handle(), original_conn);

    // Verify moved executor still works
    auto result = run_async([&executor3]() { return executor3.execute("SELECT 1"); });
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->is_success());

    // Restore executor_ for TearDown (or just set to nullptr and handle in TearDown)
    executor_ = std::make_unique<AsyncExecutor>(std::move(executor3));
}

// ============== Concurrent Operations Test ==============

TEST_F(AsyncExecutorTest, SequentialOperations) {
    // Multiple sequential operations on the same executor
    auto result1 =
        run_async([this]() { return executor_->execute("INSERT INTO test_users (name, age) VALUES ('Seq1', 30)"); });
    ASSERT_TRUE(result1.has_value() && result1->is_success());

    auto result2 =
        run_async([this]() { return executor_->execute("INSERT INTO test_users (name, age) VALUES ('Seq2', 31)"); });
    ASSERT_TRUE(result2.has_value() && result2->is_success());

    auto result3 = run_async([this]() { return executor_->execute("SELECT COUNT(*) FROM test_users"); });
    ASSERT_TRUE(result3.has_value() && result3->is_success());
    EXPECT_EQ(result3->value().rows(), 1);
}

TEST_F(AsyncExecutorTest, MultipleQueriesInSingleCoroutine) {
    const auto results = run_async(
        [this]() -> boost::asio::awaitable<std::vector<demiplane::gears::Outcome<ResultBlock, ErrorContext>>> {
            std::vector<demiplane::gears::Outcome<ResultBlock, ErrorContext>> results_;
            results_.push_back(co_await executor_->execute("INSERT INTO test_users (name) VALUES ('A')"));
            results_.push_back(co_await executor_->execute("INSERT INTO test_users (name) VALUES ('B')"));
            results_.push_back(co_await executor_->execute("SELECT COUNT(*) FROM test_users"));
            co_return results_;
        });

    // Verify all succeeded
    for (const auto& r : *results) {
        ASSERT_TRUE(r.is_success());
    }
}
