// AsyncExecutor Edge Case Tests
// Tests move semantics, moved-from state, and sequential reuse

#include <boost/asio.hpp>
#include <gtest/gtest.h>

#include "postgres_async_executor.hpp"
#include "postgres_connection_config.hpp"
#include "postgres_params.hpp"

using namespace demiplane::db::postgres;
using namespace demiplane::db;

// Test fixture for AsyncExecutor edge cases
class AsyncExecutorEdgeTest : public ::testing::Test {
protected:
    void SetUp() override {
        const auto credentials =
            ConnectionCredentials::Builder{}
                .host(demiplane::gears::value_or(std::getenv("POSTGRES_HOST"), "localhost"))
                .port(demiplane::gears::value_or(std::getenv("POSTGRES_PORT"), "5433"))
                .dbname(demiplane::gears::value_or(std::getenv("POSTGRES_DB"), "test_db"))
                .user(demiplane::gears::value_or(std::getenv("POSTGRES_USER"), "test_user"))
                .password(demiplane::gears::value_or(std::getenv("POSTGRES_PASSWORD"), "test_password"))
                .finalize();

        conn_ = PQconnectdb(credentials.to_connection_string().c_str());

        if (PQstatus(conn_) != CONNECTION_OK) {
            const std::string error = PQerrorMessage(conn_);
            PQfinish(conn_);
            conn_ = nullptr;
            GTEST_SKIP() << "Failed to connect to PostgreSQL: " << error
                         << "\nSet POSTGRES_HOST, POSTGRES_PORT, POSTGRES_DB, POSTGRES_USER, POSTGRES_PASSWORD "
                            "environment variables";
        }

        executor_ = std::make_unique<AsyncExecutor>(conn_, io_context_.get_executor());

        CreateTestTableSync();
        CleanTestTableSync();
    }

    void TearDown() override {
        if (conn_) {
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

// ============== Move Construction Tests ==============

TEST_F(AsyncExecutorEdgeTest, MoveConstruction) {
    // Verify original executor is valid
    ASSERT_TRUE(executor_->valid());
    PGconn* original_conn = executor_->native_handle();

    // Move-construct a new executor from the original
    AsyncExecutor moved_executor{std::move(*executor_)};

    // Original should now be invalid
    EXPECT_FALSE(executor_->valid());

    // Moved-to executor should be valid and hold the connection
    EXPECT_TRUE(moved_executor.valid());
    EXPECT_EQ(moved_executor.native_handle(), original_conn);

    // Moved-to executor should be able to execute queries
    auto result = run_async([&moved_executor]() { return moved_executor.execute("SELECT 1 AS number"); });

    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->is_success()) << "Query on moved-to executor failed: "
                                      << result->error<ErrorContext>().format();

    const auto& block = result->value();
    EXPECT_EQ(block.rows(), 1);

    // Destroying the moved-from executor should NOT crash
    // (it happens automatically at end of test via executor_ unique_ptr, but we can also explicitly reset)
    executor_.reset();

    // Moved-to executor should still work after the moved-from is destroyed
    auto result2 = run_async([&moved_executor]() { return moved_executor.execute("SELECT 2 AS number"); });

    ASSERT_TRUE(result2.has_value());
    ASSERT_TRUE(result2->is_success()) << "Query after moved-from destruction failed: "
                                       << result2->error<ErrorContext>().format();

    // Restore executor_ so TearDown can use the connection
    executor_ = std::make_unique<AsyncExecutor>(std::move(moved_executor));
}

// ============== Move Assignment Cleanup Tests ==============

TEST_F(AsyncExecutorEdgeTest, MoveAssignmentCleanup) {
    // Create a second connection for a second executor
    const auto credentials =
        ConnectionCredentials::Builder{}
            .host(demiplane::gears::value_or(std::getenv("POSTGRES_HOST"), "localhost"))
            .port(demiplane::gears::value_or(std::getenv("POSTGRES_PORT"), "5433"))
            .dbname(demiplane::gears::value_or(std::getenv("POSTGRES_DB"), "test_db"))
            .user(demiplane::gears::value_or(std::getenv("POSTGRES_USER"), "test_user"))
            .password(demiplane::gears::value_or(std::getenv("POSTGRES_PASSWORD"), "test_password"))
            .finalize();

    PGconn* conn2 = PQconnectdb(credentials.to_connection_string().c_str());
    ASSERT_EQ(PQstatus(conn2), CONNECTION_OK) << "Failed to create second connection";

    AsyncExecutor executor2{conn2, io_context_.get_executor()};
    ASSERT_TRUE(executor2.valid());

    PGconn* original_conn1 = executor_->native_handle();

    // Move-assign: executor2 = std::move(*executor_)
    // This should clean up executor2's previous connection resources (release socket)
    // and take over executor_'s connection
    executor2 = std::move(*executor_);

    // executor_ (moved-from) should be invalid
    EXPECT_FALSE(executor_->valid());

    // executor2 should now hold the original executor_'s connection
    EXPECT_TRUE(executor2.valid());
    EXPECT_EQ(executor2.native_handle(), original_conn1);

    // executor2 should work
    auto result = run_async([&executor2]() { return executor2.execute("SELECT 42 AS answer"); });

    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->is_success()) << "Query on move-assigned executor failed: "
                                      << result->error<ErrorContext>().format();

    const auto& block = result->value();
    EXPECT_EQ(block.rows(), 1);

    // Restore executor_ for TearDown
    executor_ = std::make_unique<AsyncExecutor>(std::move(executor2));

    // Clean up the second connection
    PQfinish(conn2);
}

// ============== Execute on Moved-From Tests ==============

TEST_F(AsyncExecutorEdgeTest, ExecuteOnMovedFrom) {
    // Move the executor away
    AsyncExecutor moved_executor{std::move(*executor_)};

    // The moved-from executor should be invalid
    ASSERT_FALSE(executor_->valid());

    // Attempting to execute on the moved-from executor should return an error
    auto result = run_async([this]() { return executor_->execute("SELECT 1"); });

    ASSERT_TRUE(result.has_value());
    ASSERT_FALSE(result->is_success()) << "Execute on moved-from executor should fail";

    const auto& error = result->error<ErrorContext>();
    EXPECT_TRUE(error.code.is_client_error());
    EXPECT_FALSE(error.message.empty());

    // Restore executor_ for TearDown
    executor_ = std::make_unique<AsyncExecutor>(std::move(moved_executor));
}

// ============== Sequential Reuse Tests ==============

TEST_F(AsyncExecutorEdgeTest, SequentialReuse) {
    // Execute 25 queries on the same executor sequentially, all should succeed
    constexpr int query_count = 25;

    for (int i = 0; i < query_count; ++i) {
        const std::string insert_sql = "INSERT INTO test_users (name, age) VALUES ('SeqUser" + std::to_string(i) +
                                       "', " + std::to_string(20 + i) + ")";

        auto result = run_async([this, &insert_sql]() { return executor_->execute(insert_sql); });

        ASSERT_TRUE(result.has_value()) << "No result at iteration " << i;
        ASSERT_TRUE(result->is_success())
            << "Insert failed at iteration " << i << ": " << result->error<ErrorContext>().format();
    }

    // Verify all rows were inserted
    auto count_result = run_async([this]() { return executor_->execute("SELECT COUNT(*) FROM test_users"); });

    ASSERT_TRUE(count_result.has_value());
    ASSERT_TRUE(count_result->is_success()) << "Count query failed: " << count_result->error<ErrorContext>().format();

    const auto& block = count_result->value();
    EXPECT_EQ(block.rows(), 1);

    // Also verify with a SELECT to confirm data integrity
    auto select_result =
        run_async([this]() { return executor_->execute("SELECT name, age FROM test_users ORDER BY age"); });

    ASSERT_TRUE(select_result.has_value());
    ASSERT_TRUE(select_result->is_success())
        << "Select query failed: " << select_result->error<ErrorContext>().format();

    EXPECT_EQ(select_result->value().rows(), query_count);
}

// ============== Null Construction Tests ==============

TEST_F(AsyncExecutorEdgeTest, NullConstruction) {
    // Construct with nullptr connection
    AsyncExecutor null_executor{nullptr, io_context_.get_executor()};

    // Should be invalid
    EXPECT_FALSE(null_executor.valid());
    EXPECT_EQ(null_executor.native_handle(), nullptr);

    // Attempting to execute should return an error
    auto result = run_async([&null_executor]() { return null_executor.execute("SELECT 1"); });

    ASSERT_TRUE(result.has_value());
    ASSERT_FALSE(result->is_success()) << "Execute on null-constructed executor should fail";

    const auto& error = result->error<ErrorContext>();
    EXPECT_TRUE(error.code.is_client_error());
    EXPECT_EQ(error.code, ClientErrorCode::NotConnected);
    EXPECT_FALSE(error.message.empty());
}

// ============== Coroutine Lifetime Safety Tests ==============
// These tests verify that variadic/tuple/compiled-static execute overloads
// safely capture temporaries by value into the coroutine frame.
// Boost.Asio awaitables are lazy (suspend at initial_suspend), so reference
// parameters to temporaries would dangle before the coroutine body runs.

TEST_F(AsyncExecutorEdgeTest, VariadicTemporaryString) {
    // Temporary std::string passed as variadic arg — must be captured by value
    run_async([this]() { return executor_->execute("INSERT INTO test_users (name, age) VALUES ('Tmp', 20)"); });

    auto result = run_async(
        [this]() { return executor_->execute("SELECT name FROM test_users WHERE name = $1", std::string{"Tmp"}); });

    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->is_success()) << "Temporary string arg failed: " << result->error<ErrorContext>().format();
    EXPECT_EQ(result->value().rows(), 1);
}

TEST_F(AsyncExecutorEdgeTest, VariadicMultipleTemporaries) {
    // Multiple temporaries of mixed types — all captured by value
    auto result = run_async([this]() {
        return executor_->execute("INSERT INTO test_users (name, age, email, active) VALUES ($1, $2, $3, $4)",
                                  std::string{"TmpMulti"},
                                  33,
                                  std::string{"tmp@test.com"},
                                  true);
    });

    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->is_success()) << "Multiple temporaries failed: " << result->error<ErrorContext>().format();

    // Verify data was inserted correctly
    auto select = run_async([this]() {
        return executor_->execute("SELECT name, age, email FROM test_users WHERE name = $1", std::string{"TmpMulti"});
    });
    ASSERT_TRUE(select.has_value() && select->is_success());
    EXPECT_EQ(select->value().rows(), 1);
}

TEST_F(AsyncExecutorEdgeTest, VariadicMovedString) {
    // std::move'd string — must survive coroutine lazy start
    std::string name = "MovedUser";
    auto result      = run_async([this, name = std::move(name)]() mutable {
        return executor_->execute("INSERT INTO test_users (name, age) VALUES ($1, $2)", std::move(name), 40);
    });

    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->is_success()) << "Moved string failed: " << result->error<ErrorContext>().format();
}

TEST_F(AsyncExecutorEdgeTest, TupleTemporaries) {
    // Tuple of temporaries — tuple taken by value, delegates to variadic
    run_async([this]() { return executor_->execute("INSERT INTO test_users (name, age) VALUES ('TupleUser', 50)"); });

    auto result = run_async([this]() {
        return executor_->execute("SELECT name FROM test_users WHERE name = $1 AND age = $2",
                                  std::tuple{std::string{"TupleUser"}, 50});
    });

    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->is_success()) << "Tuple temporaries failed: " << result->error<ErrorContext>().format();
    EXPECT_EQ(result->value().rows(), 1);
}

TEST_F(AsyncExecutorEdgeTest, CompiledStaticQueryTemporary) {
    // CompiledStaticQuery temporary — taken by value, delegates via co_return co_await
    run_async([this]() { return executor_->execute("INSERT INTO test_users (name, age) VALUES ('CSQUser', 55)"); });

    auto result = run_async([this]() {
        return executor_->execute(CompiledStaticQuery{
            std::string{R"(SELECT "name" FROM "test_users" WHERE "name" = $1)"}, std::tuple{std::string{"CSQUser"}}});
    });

    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->is_success()) << "CompiledStaticQuery temporary failed: "
                                      << result->error<ErrorContext>().format();
    EXPECT_EQ(result->value().rows(), 1);
}

TEST_F(AsyncExecutorEdgeTest, VariadicEmptyArgs) {
    // Zero variadic args — should resolve to simple query overload, no lifetime issue
    auto result = run_async([this]() { return executor_->execute("SELECT 1 AS number"); });

    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->is_success()) << "Empty variadic failed: " << result->error<ErrorContext>().format();
    EXPECT_EQ(result->value().rows(), 1);
}
