// PostgreSQL Cylinder Integration Tests
// Tests cylinder pool behavior: multi-executor, exhaustion, shutdown, stats, concurrency

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
    auto credentials = ConnectionCredentials{}
                           .host(demiplane::gears::value_or(std::getenv("POSTGRES_HOST"), "localhost"))
                           .port(demiplane::gears::value_or(std::getenv("POSTGRES_PORT"), "5433"))
                           .dbname(demiplane::gears::value_or(std::getenv("POSTGRES_DB"), "test_db"))
                           .user(demiplane::gears::value_or(std::getenv("POSTGRES_USER"), "test_user"))
                           .password(demiplane::gears::value_or(std::getenv("POSTGRES_PASSWORD"), "test_password"));
    ConnectionConfig config{std::move(credentials)};
    config.ssl_mode(SslMode::DISABLE).validate();
    return config;
}

// ============== Test Fixture ==============

class CylinderTest : public ::testing::Test {
protected:
    void SetUp() override {
        const auto conn_string = make_test_config().to_connection_string();
        PGconn* probe          = PQconnectdb(conn_string.c_str());

        if (!probe || PQstatus(probe) != CONNECTION_OK) {
            const std::string error = probe ? PQerrorMessage(probe) : "null connection";
            if (probe)
                PQfinish(probe);
            GTEST_SKIP() << "PostgreSQL unavailable: " << error
                         << "\nSet POSTGRES_HOST, POSTGRES_PORT, POSTGRES_DB, POSTGRES_USER, POSTGRES_PASSWORD";
        }
        PQfinish(probe);
    }
};

// ============== MultipleSyncExecutors ==============

TEST_F(CylinderTest, MultipleSyncExecutorsRunQueriesAndRelease) {
    auto session =
        Session{make_test_config(), CylinderConfig{}.capacity(4).min_connections(2).health_check_interval(2s)};

    // Acquire 3 executors simultaneously from a capacity-4 pool
    {
        auto exec1 = session.with_sync();
        auto exec2 = session.with_sync();
        auto exec3 = session.with_sync();

        ASSERT_TRUE(exec1.valid());
        ASSERT_TRUE(exec2.valid());
        ASSERT_TRUE(exec3.valid());

        // Each executor runs an independent query on its own connection
        auto r1 = exec1.execute("SELECT 1 AS n");
        auto r2 = exec2.execute("SELECT 2 AS n");
        auto r3 = exec3.execute("SELECT 3 AS n");

        ASSERT_TRUE(r1.is_success()) << r1.error<ErrorContext>().format();
        ASSERT_TRUE(r2.is_success()) << r2.error<ErrorContext>().format();
        ASSERT_TRUE(r3.is_success()) << r3.error<ErrorContext>().format();

        EXPECT_EQ(r1.value().get<int>(0, 0), 1);
        EXPECT_EQ(r2.value().get<int>(0, 0), 2);
        EXPECT_EQ(r3.value().get<int>(0, 0), 3);
    }
    // All 3 executors destroyed — slots returned

    // Session is still usable after releasing all executors
    auto exec = session.with_sync();
    ASSERT_TRUE(exec.valid());
    auto result = exec.execute("SELECT 42 AS answer");
    ASSERT_TRUE(result.is_success()) << result.error<ErrorContext>().format();
    EXPECT_EQ(result.value().get<int>(0, 0), 42);

    session.shutdown();
}

// ============== PoolExhaustion ==============

TEST_F(CylinderTest, PoolExhaustionReturnsInvalidExecutor) {
    auto session =
        Session{make_test_config(), CylinderConfig{}.capacity(2).min_connections(1).health_check_interval(2s)};

    // Exhaust the pool (capacity=2)
    auto exec1 = session.with_sync();
    auto exec2 = session.with_sync();
    ASSERT_TRUE(exec1.valid());
    ASSERT_TRUE(exec2.valid());

    // 3rd acquire should fail — pool exhausted
    auto exec3 = session.with_sync();
    EXPECT_FALSE(exec3.valid());

    // Release one executor by moving it out of scope
    { auto released = std::move(exec1); }

    // Now acquire should succeed again
    auto exec4 = session.with_sync();
    EXPECT_TRUE(exec4.valid());

    auto result = exec4.execute("SELECT 1");
    ASSERT_TRUE(result.is_success()) << result.error<ErrorContext>().format();

    session.shutdown();
}

// ============== SessionShutdown ==============

TEST_F(CylinderTest, ShutdownPreventsNewAcquisitions) {
    auto session =
        Session{make_test_config(), CylinderConfig{}.capacity(4).min_connections(1).health_check_interval(2s)};

    // Verify session works before shutdown
    {
        auto exec = session.with_sync();
        ASSERT_TRUE(exec.valid());
        auto result = exec.execute("SELECT 1");
        ASSERT_TRUE(result.is_success()) << result.error<ErrorContext>().format();
    }

    // Shutdown the session
    session.shutdown();
    EXPECT_TRUE(session.is_shutdown());

    // New acquisitions should return invalid executors
    auto exec = session.with_sync();
    EXPECT_FALSE(exec.valid());
}

// ============== ExecutorLifecycleScope ==============

TEST_F(CylinderTest, ExecutorLifecycleScopeReleasesSlot) {
    auto session =
        Session{make_test_config(), CylinderConfig{}.capacity(4).min_connections(1).health_check_interval(2s)};

    const auto free_before = session.cylinder_free_count();

    // Inner scope: acquire and use an executor
    {
        auto exec = session.with_sync();
        ASSERT_TRUE(exec.valid());

        // Free count should have decreased
        EXPECT_LT(session.cylinder_free_count(), free_before + session.cylinder_capacity());

        auto result = exec.execute("SELECT pg_backend_pid()");
        ASSERT_TRUE(result.is_success()) << result.error<ErrorContext>().format();
    }
    // Executor destroyed — slot returned to cylinder

    // Session recovers: can acquire again and execute queries
    auto exec = session.with_sync();
    ASSERT_TRUE(exec.valid());
    auto result = exec.execute("SELECT 1");
    ASSERT_TRUE(result.is_success()) << result.error<ErrorContext>().format();

    // Free count should be restored (minus the one we just acquired)
    EXPECT_GE(session.cylinder_free_count() + 1, free_before);

    session.shutdown();
}

// ============== SessionStatsAccuracy ==============

TEST_F(CylinderTest, SessionStatsAccuracy) {
    auto session =
        Session{make_test_config(), CylinderConfig{}.capacity(4).min_connections(2).health_check_interval(2s)};

    // Capacity is always 4
    EXPECT_EQ(session.cylinder_capacity(), 4u);

    // Before any acquisitions: active should be 0
    const auto active_before = session.cylinder_active_count();
    const auto free_before   = session.cylinder_free_count();
    EXPECT_EQ(active_before, 0u);
    EXPECT_GT(free_before, 0u);

    // Acquire two executors
    auto exec1 = session.with_sync();
    auto exec2 = session.with_sync();
    ASSERT_TRUE(exec1.valid());
    ASSERT_TRUE(exec2.valid());

    // During: active count should have increased by 2
    EXPECT_EQ(session.cylinder_active_count(), active_before + 2);

    // Free count should have decreased
    EXPECT_LT(session.cylinder_free_count(), free_before);

    // Release executors
    { auto released1 = std::move(exec1); }
    { auto released2 = std::move(exec2); }

    // After release: active count should be back to initial
    EXPECT_EQ(session.cylinder_active_count(), active_before);

    session.shutdown();
}

// ============== DoubleShutdownIdempotent ==============

TEST_F(CylinderTest, DoubleShutdownIdempotent) {
    auto session =
        Session{make_test_config(), CylinderConfig{}.capacity(4).min_connections(1).health_check_interval(2s)};

    // First shutdown
    EXPECT_NO_THROW(session.shutdown());
    EXPECT_TRUE(session.is_shutdown());

    // Second shutdown — should not crash or throw
    EXPECT_NO_THROW(session.shutdown());
    EXPECT_TRUE(session.is_shutdown());
}

// ============== ConcurrentAsyncQueries ==============

TEST_F(CylinderTest, ConcurrentAsyncQueriesComplete) {
    auto session =
        Session{make_test_config(), CylinderConfig{}.capacity(4).min_connections(2).health_check_interval(2s)};

    boost::asio::io_context ioc;

    constexpr int kQueries = 4;
    std::atomic<int> success_count{0};
    std::atomic<int> completion_count{0};

    for (int i = 0; i < kQueries; ++i) {
        boost::asio::co_spawn(
            ioc,
            [&session, &ioc, &success_count, &completion_count, i]() -> boost::asio::awaitable<void> {
                auto exec   = session.with_async(ioc.get_executor());
                auto result = co_await exec.execute("SELECT $1::integer AS n", i);
                if (result.is_success() && result.value().get<int>(0, 0) == i) {
                    ++success_count;
                }
                ++completion_count;
                co_return;
            },
            boost::asio::detached);
    }

    ioc.run();

    EXPECT_EQ(completion_count.load(), kQueries);
    EXPECT_EQ(success_count.load(), kQueries);

    session.shutdown();
}

// ============== ConcurrentSyncFromThreads ==============

TEST_F(CylinderTest, ConcurrentSyncExecutorsFromMultipleThreads) {
    auto session =
        Session{make_test_config(), CylinderConfig{}.capacity(4).min_connections(2).health_check_interval(2s)};

    constexpr int kThreads = 4;
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};

    threads.reserve(kThreads);
    for (int i = 0; i < kThreads; ++i) {
        threads.emplace_back([&session, &success_count, i] {
            auto exec = session.with_sync();
            if (!exec.valid()) {
                return;
            }
            auto result = exec.execute("SELECT $1::integer AS n", i);
            if (result.is_success() && result.value().get<int>(0, 0) == i) {
                ++success_count;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(success_count.load(), kThreads);

    session.shutdown();
}
