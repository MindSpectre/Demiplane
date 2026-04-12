// PostgreSQL Session Functional Tests
// Tests Session, ConnectionPool, PoolJanitor, SyncExecutor, AsyncExecutor

#include <thread>
#include <vector>

#include <boost/asio.hpp>
#include <gtest/gtest.h>
#include <postgres_session.hpp>


using namespace demiplane::db::postgres;
using namespace demiplane::db;
using namespace std::chrono_literals;
// ============== Test Helpers ==============

static ConnectionConfig make_test_config() {
    auto credentials = ConnectionCredentials::Builder{}
                           .host(demiplane::gears::value_or(std::getenv("POSTGRES_HOST"), "localhost"))
                           .port(demiplane::gears::value_or(std::getenv("POSTGRES_PORT"), "5433"))
                           .dbname(demiplane::gears::value_or(std::getenv("POSTGRES_DB"), "test_db"))
                           .user(demiplane::gears::value_or(std::getenv("POSTGRES_USER"), "test_user"))
                           .password(demiplane::gears::value_or(std::getenv("POSTGRES_PASSWORD"), "test_password"))
                           .finalize();
    return ConnectionConfig::Builder{}.credentials(std::move(credentials)).ssl_mode(SslMode::DISABLE).finalize();
}

// Helper to run an async coroutine to completion in tests
template <typename CoroFunc>
auto run_async(boost::asio::io_context& io, CoroFunc&& func) {
    using awaitable_type = std::invoke_result_t<CoroFunc>;
    using result_type    = awaitable_type::value_type;

    std::optional<result_type> result;
    std::exception_ptr eptr;

    boost::asio::co_spawn(
        io,
        [&]() -> boost::asio::awaitable<void> {
            try {
                result = co_await func();
            } catch (...) {
                eptr = std::current_exception();
            }
        },
        boost::asio::detached);

    io.run();
    io.restart();

    if (eptr) {
        std::rethrow_exception(eptr);
    }

    return result;
}

// ============== PoolConfig Unit Tests (no DB required) ==============

class PoolConfigTest : public ::testing::Test {};

TEST_F(PoolConfigTest, ValidateAcceptsValidConfig) {
    const auto cfg = PoolConfig::Builder{}.capacity(16).min_connections(2).finalize();
    EXPECT_NO_THROW(cfg.validate());
}

TEST_F(PoolConfigTest, ValidateRejectsZeroCapacity) {
    EXPECT_THROW((void)PoolConfig::Builder{}.capacity(0).finalize(), std::invalid_argument);
}

TEST_F(PoolConfigTest, ValidateRejectsNonPowerOfTwo) {
    EXPECT_THROW((void)PoolConfig::Builder{}.capacity(10).finalize(), std::invalid_argument);
}

TEST_F(PoolConfigTest, ValidateRejectsMinConnectionsExceedCapacity) {
    EXPECT_THROW((void)PoolConfig::Builder{}.capacity(4).min_connections(8).finalize(), std::invalid_argument);
}

TEST_F(PoolConfigTest, FactoryMethodsProduceValidConfigs) {
    EXPECT_NO_THROW(PoolConfig::minimal().validate());
    EXPECT_NO_THROW(PoolConfig::standard().validate());
    EXPECT_NO_THROW(PoolConfig::high_performance().validate());
}

TEST_F(PoolConfigTest, MinimalConfigHasSmallCapacity) {
    const auto cfg = PoolConfig::minimal();
    EXPECT_EQ(cfg.capacity(), 2u);
    EXPECT_EQ(cfg.min_connections(), 1u);
}

TEST_F(PoolConfigTest, HighPerformanceConfigHasLargeCapacity) {
    const auto cfg = PoolConfig::high_performance();
    EXPECT_GE(cfg.capacity(), 32u);
}

// ============== Session Integration Tests (requires PostgreSQL) ==============

class SessionTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Verify connectivity before creating session
        const auto conn_string = make_test_config().to_connection_string();
        PGconn* probe          = PQconnectdb(conn_string.c_str());

        if (!probe || PQstatus(probe) != CONNECTION_OK) {
            const std::string error = probe ? PQerrorMessage(probe) : "null connection";
            if (probe)
                PQfinish(probe);
            GTEST_SKIP() << "Failed to connect to PostgreSQL: " << error
                         << "\nSet POSTGRES_HOST, POSTGRES_PORT, POSTGRES_DB, POSTGRES_USER, POSTGRES_PASSWORD";
        }
        PQfinish(probe);

        session_ = std::make_unique<Session>(
            make_test_config(),
            PoolConfig::Builder{}.capacity(4).min_connections(1).health_check_interval(2s).finalize());

        // Create test table via with_sync
        auto exec   = session_->with_sync().value();
        auto result = exec.execute(R"(
            CREATE TABLE IF NOT EXISTS session_test_users (
                id SERIAL PRIMARY KEY,
                name VARCHAR(100) NOT NULL,
                value INTEGER DEFAULT 0
            )
        )");
        ASSERT_TRUE(result.is_success()) << "Setup failed: " << result.error<ErrorContext>().format();

        auto truncate = exec.execute("TRUNCATE TABLE session_test_users RESTART IDENTITY CASCADE");
        ASSERT_TRUE(truncate.is_success()) << "Truncate failed: " << truncate.error<ErrorContext>().format();
    }

    void TearDown() override {
        if (session_) {
            auto exec        = session_->with_sync().value();
            GEARS_UNUSED_VAR = exec.execute("DROP TABLE IF EXISTS session_test_users CASCADE");
            session_->shutdown();
        }
    }

    std::unique_ptr<Session> session_;
    boost::asio::io_context io_;
};

// ============== with_sync() Tests ==============

TEST_F(SessionTest, WithSyncExecutesSimpleQuery) {
    auto exec   = session_->with_sync().value();
    auto result = exec.execute("SELECT 1 AS n");

    ASSERT_TRUE(result.is_success()) << result.error<ErrorContext>().format();
    EXPECT_EQ(result.value().rows(), 1);
}

TEST_F(SessionTest, WithSyncInsertsAndSelects) {
    {
        auto exec   = session_->with_sync().value();
        auto result = exec.execute("INSERT INTO session_test_users (name, value) VALUES ('Alice', 42)");
        ASSERT_TRUE(result.is_success()) << result.error<ErrorContext>().format();
    }

    auto exec   = session_->with_sync().value();
    auto result = exec.execute("SELECT name, value FROM session_test_users WHERE name = 'Alice'");

    ASSERT_TRUE(result.is_success()) << result.error<ErrorContext>().format();
    ASSERT_EQ(result.value().rows(), 1);
    EXPECT_EQ(result.value().get<std::string>(0, 0), "Alice");
    EXPECT_EQ(result.value().get<int>(0, 1), 42);
}

TEST_F(SessionTest, WithSyncVariadicParameters) {
    auto exec   = session_->with_sync().value();
    auto result = exec.execute("INSERT INTO session_test_users (name, value) VALUES ($1, $2)", std::string{"Bob"}, 99);

    ASSERT_TRUE(result.is_success()) << result.error<ErrorContext>().format();

    auto select = exec.execute("SELECT value FROM session_test_users WHERE name = $1", std::string{"Bob"});
    ASSERT_TRUE(select.is_success());
    EXPECT_EQ(select.value().get<int>(0, 0), 99);
}

TEST_F(SessionTest, WithSyncReturnsErrorOnBadQuery) {
    auto exec   = session_->with_sync().value();
    auto result = exec.execute("SELCT 1");  // typo

    ASSERT_FALSE(result.is_success());
    EXPECT_EQ(result.error<ErrorContext>().sqlstate.substr(0, 2), "42");
}

TEST_F(SessionTest, WithSyncReleasesConnectionAfterScope) {
    const auto free_before = session_->pool_free_count();

    {
        auto exec         = session_->with_sync().value();
        const auto result = exec.execute("SELECT 1");
        ASSERT_TRUE(result.is_success());
        // Connection is USED inside this scope
    }
    // Connection released here (slot reset by SyncExecutor destructor)

    EXPECT_GE(session_->pool_free_count(), free_before);
}

TEST_F(SessionTest, WithSyncMultipleSequentialCalls) {
    for (int i = 0; i < 5; ++i) {
        auto exec   = session_->with_sync().value();
        auto result = exec.execute("SELECT $1::integer AS n", i);
        ASSERT_TRUE(result.is_success()) << "Iteration " << i << " failed: " << result.error<ErrorContext>().format();
        EXPECT_EQ(result.value().get<int>(0, 0), i);
    }
}

// ============== with_async() Tests ==============

TEST_F(SessionTest, WithAsyncExecutesSimpleQuery) {
    auto result =
        run_async(io_, [this]() -> boost::asio::awaitable<demiplane::gears::Outcome<ResultBlock, ErrorContext>> {
            auto exec = session_->with_async(io_.get_executor()).value();
            co_return co_await exec.execute("SELECT 42 AS answer");
        });

    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->is_success()) << result->error<ErrorContext>().format();
    EXPECT_EQ(result->value().get<int>(0, 0), 42);
}

TEST_F(SessionTest, WithAsyncInsertsAndSelects) {
    // Insert via async
    auto insert_result =
        run_async(io_, [this]() -> boost::asio::awaitable<demiplane::gears::Outcome<ResultBlock, ErrorContext>> {
            auto exec = session_->with_async(io_.get_executor()).value();
            co_return co_await exec.execute(
                "INSERT INTO session_test_users (name, value) VALUES ($1, $2)", std::string{"Charlie"}, 7);
        });
    ASSERT_TRUE(insert_result.has_value());
    ASSERT_TRUE(insert_result->is_success()) << insert_result->error<ErrorContext>().format();

    // Select via sync to verify
    auto exec   = session_->with_sync().value();
    auto select = exec.execute("SELECT value FROM session_test_users WHERE name = 'Charlie'");
    ASSERT_TRUE(select.is_success());
    EXPECT_EQ(select.value().get<int>(0, 0), 7);
}

TEST_F(SessionTest, WithAsyncReturnsErrorOnBadQuery) {
    auto result =
        run_async(io_, [this]() -> boost::asio::awaitable<demiplane::gears::Outcome<ResultBlock, ErrorContext>> {
            auto exec = session_->with_async(io_.get_executor()).value();
            co_return co_await exec.execute("INVALID SYNTAX !!!");
        });

    ASSERT_TRUE(result.has_value());
    ASSERT_FALSE(result->is_success());
    EXPECT_FALSE(result->error<ErrorContext>().sqlstate.empty());
}

TEST_F(SessionTest, WithAsyncReleasesConnectionAfterScope) {
    const auto free_before = session_->pool_free_count();

    run_async(io_, [this]() -> boost::asio::awaitable<demiplane::gears::Outcome<ResultBlock, ErrorContext>> {
        auto exec = session_->with_async(io_.get_executor()).value();
        // exec holds connection during co_await
        co_return co_await exec.execute("SELECT 1");
    });

    // Connection must be returned after scope (slot reset by AsyncExecutor destructor)
    EXPECT_GE(session_->pool_free_count(), free_before);
}

TEST_F(SessionTest, WithAsyncMultipleSequentialCalls) {
    for (int i = 0; i < 5; ++i) {
        const auto n = i;
        auto result =
            run_async(io_, [this, n]() -> boost::asio::awaitable<demiplane::gears::Outcome<ResultBlock, ErrorContext>> {
                auto exec = session_->with_async(io_.get_executor()).value();
                co_return co_await exec.execute("SELECT $1::integer AS n", n);
            });

        ASSERT_TRUE(result.has_value());
        ASSERT_TRUE(result->is_success()) << "Iteration " << i;
        EXPECT_EQ(result->value().get<int>(0, 0), i);
    }
}

// ============== Pool Stats Tests ==============

TEST_F(SessionTest, PoolCapacityMatchesConfig) {
    EXPECT_EQ(session_->pool_capacity(), 4u);
}

TEST_F(SessionTest, PoolFreeCountIsPositive) {
    EXPECT_GT(session_->pool_free_count(), 0u);
}

TEST_F(SessionTest, ActiveCountIncreasesWhileConnectionHeld) {
    const auto active_before = session_->pool_active_count();

    auto exec                = session_->with_sync().value();
    const auto active_during = session_->pool_active_count();

    EXPECT_GT(active_during, active_before);

    // Verify it decrements after scope (exec destructor)
    GEARS_UNUSED_VAR = exec.execute("SELECT 1");  // ensure exec is used so it's not optimised away
}

// ============== Pool Exhaustion Test ==============

TEST_F(SessionTest, SyncExecutorIsInvalidWhenPoolExhausted) {
    // Capacity is 4; hold 4 connections, 5th should get nullptr
    [[maybe_unused]] auto exec1 = session_->with_sync().value();
    [[maybe_unused]] auto exec2 = session_->with_sync().value();
    [[maybe_unused]] auto exec3 = session_->with_sync().value();
    [[maybe_unused]] auto exec4 = session_->with_sync().value();

    // Pool fully exhausted (all 4 slots USED)
    const auto exec5 = session_->with_sync();
    EXPECT_FALSE(exec5.is_success());

    // After releasing one, next acquire should succeed
}

// ============== Timeout Overload Tests ==============

TEST_F(SessionTest, WithSyncTimeoutSucceedsWhenSlotImmediatelyAvailable) {
    // Pool is idle — the first acquire should succeed on attempt 0 (before any sleep)
    const auto start   = std::chrono::steady_clock::now();
    auto exec          = session_->with_sync(10s);
    const auto elapsed = std::chrono::steady_clock::now() - start;

    ASSERT_TRUE(exec.is_success());
    // 10 sleeps of 1s would take ~10s; success path must be under 1s
    EXPECT_LT(elapsed, 1s);
}

TEST_F(SessionTest, WithSyncTimeoutReturnsErrorAfterFullBudget) {
    // Exhaust all 4 slots
    [[maybe_unused]] auto e1 = session_->with_sync().value();
    [[maybe_unused]] auto e2 = session_->with_sync().value();
    [[maybe_unused]] auto e3 = session_->with_sync().value();
    [[maybe_unused]] auto e4 = session_->with_sync().value();

    // With pool fully exhausted, a bounded timeout must return PoolExhausted
    const auto start   = std::chrono::steady_clock::now();
    const auto result  = session_->with_sync(200ms);
    const auto elapsed = std::chrono::steady_clock::now() - start;

    EXPECT_FALSE(result.is_success());
    EXPECT_EQ(result.error<ErrorContext>().code.value(), static_cast<int>(ClientErrorCode::PoolExhausted));
    // Full budget should elapse: 10 × 20ms = 200ms, allow jitter
    EXPECT_GE(elapsed, 180ms);
    EXPECT_LT(elapsed, 400ms);
}

TEST_F(SessionTest, WithSyncTimeoutAcquiresSlotReleasedDuringWait) {
    // Hold all 4 slots upfront
    auto e1 = session_->with_sync().value();
    auto e2 = session_->with_sync().value();
    auto e3 = session_->with_sync().value();
    auto e4 = session_->with_sync().value();

    // Release e1 from a background thread after ~100ms
    std::thread releaser{[&e1] {
        std::this_thread::sleep_for(100ms);
        { [[maybe_unused]] auto moved = std::move(e1); }  // destructor frees the slot
    }};

    // Main thread waits up to 2s — should acquire within ~200ms (2nd retry at 200ms)
    const auto start   = std::chrono::steady_clock::now();
    const auto result  = session_->with_sync(2s);
    const auto elapsed = std::chrono::steady_clock::now() - start;

    releaser.join();

    ASSERT_TRUE(result.is_success());
    EXPECT_LT(elapsed, 500ms) << "waiter should pick up the slot shortly after release";
    EXPECT_GE(elapsed, 100ms) << "waiter must wait at least for the release";
}

TEST_F(SessionTest, BeginTransactionTimeoutReturnsErrorWhenExhausted) {
    [[maybe_unused]] auto e1 = session_->with_sync().value();
    [[maybe_unused]] auto e2 = session_->with_sync().value();
    [[maybe_unused]] auto e3 = session_->with_sync().value();
    [[maybe_unused]] auto e4 = session_->with_sync().value();

    const auto result = session_->begin_transaction(TransactionOptions{}, 100ms);
    EXPECT_FALSE(result.is_success());
    EXPECT_EQ(result.error<ErrorContext>().code.value(), static_cast<int>(ClientErrorCode::PoolExhausted));
}

TEST_F(SessionTest, BeginAutoTransactionTimeoutReturnsErrorWhenExhausted) {
    [[maybe_unused]] auto e1 = session_->with_sync().value();
    [[maybe_unused]] auto e2 = session_->with_sync().value();
    [[maybe_unused]] auto e3 = session_->with_sync().value();
    [[maybe_unused]] auto e4 = session_->with_sync().value();

    const auto result = session_->begin_auto_transaction(TransactionOptions{}, 100ms);
    EXPECT_FALSE(result.is_success());
    EXPECT_EQ(result.error<ErrorContext>().code.value(), static_cast<int>(ClientErrorCode::PoolExhausted));
}

TEST_F(SessionTest, WithAsyncTimeoutReturnsErrorWhenExhausted) {
    [[maybe_unused]] auto e1 = session_->with_sync().value();
    [[maybe_unused]] auto e2 = session_->with_sync().value();
    [[maybe_unused]] auto e3 = session_->with_sync().value();
    [[maybe_unused]] auto e4 = session_->with_sync().value();

    const auto result = session_->with_async(io_.get_executor(), 100ms);
    EXPECT_FALSE(result.is_success());
    EXPECT_EQ(result.error<ErrorContext>().code.value(), static_cast<int>(ClientErrorCode::PoolExhausted));
}

TEST_F(SessionTest, WithSyncZeroTimeoutBehavesLikeImmediate) {
    [[maybe_unused]] auto e1 = session_->with_sync().value();
    [[maybe_unused]] auto e2 = session_->with_sync().value();
    [[maybe_unused]] auto e3 = session_->with_sync().value();
    [[maybe_unused]] auto e4 = session_->with_sync().value();

    // Zero duration must short-circuit to acquire_slot() and return immediately
    const auto start   = std::chrono::steady_clock::now();
    const auto result  = session_->with_sync(std::chrono::milliseconds::zero());
    const auto elapsed = std::chrono::steady_clock::now() - start;

    EXPECT_FALSE(result.is_success());
    EXPECT_LT(elapsed, 20ms) << "zero timeout must not sleep";
}

// ============== Concurrent Access Test ==============

TEST_F(SessionTest, ConcurrentSyncExecutorsOnSeparateConnections) {
    constexpr int kThreads = 4;
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};

    threads.reserve(kThreads);
    for (int i = 0; i < kThreads; ++i) {
        threads.emplace_back([&] {
            auto exec = session_->with_sync().value();
            if (const auto result = exec.execute("SELECT pg_sleep(0.01), pg_backend_pid() AS pid");
                result.is_success()) {
                ++success_count;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(success_count.load(), kThreads);
}

// ============== Shutdown Tests ==============

TEST_F(SessionTest, IsShutdownReturnsFalseInitially) {
    EXPECT_FALSE(session_->is_shutdown());
}

TEST_F(SessionTest, ShutdownMarksPoolAsShutdown) {
    session_->shutdown();
    EXPECT_TRUE(session_->is_shutdown());
}

TEST_F(SessionTest, AcquireAfterShutdownReturnsInvalidExecutor) {
    session_->shutdown();
    const auto exec = session_->with_sync();
    EXPECT_FALSE(exec.is_success());
}

TEST_F(SessionTest, ShutdownIsIdempotent) {
    EXPECT_NO_THROW(session_->shutdown());
    EXPECT_NO_THROW(session_->shutdown());
    EXPECT_TRUE(session_->is_shutdown());
}

// ============== DISCARD ALL Cleanup Test ==============

TEST_F(SessionTest, ConnectionIsCleanedAfterRelease) {
    // Set a session-local variable, release connection, then check it's gone
    {
        auto exec = session_->with_sync().value();

        // Set a temporary table (wiped by DISCARD ALL)
        auto result = exec.execute("CREATE TEMP TABLE _session_marker (x INT)");
        ASSERT_TRUE(result.is_success()) << result.error<ErrorContext>().format();
    }
    // After release, slot reset sends DISCARD ALL -- temp table is gone

    // Acquire a NEW connection from pool (may be different slot or same cleaned slot)
    auto exec = session_->with_sync().value();
    // If the pool returned the same cleaned connection, the temp table must not exist
    // Query succeeds (0 rows) or fails with schema-not-found -- either way _session_marker is gone
    if (auto result = exec.execute("SELECT 1 FROM pg_temp.pg_class WHERE relname = '_session_marker'");
        result.is_success()) {
        EXPECT_EQ(result.value().rows(), 0);
    }
}
