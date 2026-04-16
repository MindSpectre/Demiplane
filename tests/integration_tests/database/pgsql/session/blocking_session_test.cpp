// PostgreSQL BlockingSession Functional Tests
// Tests BlockingSession, BlockingPool, QueuedHolder, FIFO waiter semantics.

#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>
#include <vector>

#include <gtest/gtest.h>
#include <postgres_blocking_session.hpp>

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

static PoolConfig make_pool_config(std::size_t cap, std::size_t min) {
    return PoolConfig::Builder{}.capacity(cap).min_connections(min).finalize();
}

// ============== BlockingSession Fixture ==============

class BlockingSessionTest : public ::testing::Test {
protected:
    void SetUp() override {
        const auto conn_string = make_test_config().to_connection_string();
        PGconn* probe          = PQconnectdb(conn_string.c_str());
        if (!probe || PQstatus(probe) != CONNECTION_OK) {
            const std::string error = probe ? PQerrorMessage(probe) : "null connection";
            if (probe)
                PQfinish(probe);
            GTEST_SKIP() << "Failed to connect to PostgreSQL: " << error;
        }
        PQfinish(probe);
    }
};

// ============== Basic Execution ==============

TEST_F(BlockingSessionTest, TryWithSyncExecutesSimpleQuery) {
    BlockingSession session{make_test_config(), make_pool_config(2, 1)};

    auto outcome = session.try_with_sync();
    ASSERT_TRUE(outcome.is_success());

    auto exec   = std::move(outcome).value();
    auto result = exec.execute("SELECT 42 AS answer");
    ASSERT_TRUE(result.is_success());
    EXPECT_EQ(result.value().rows(), 1u);
}

TEST_F(BlockingSessionTest, WithSyncTimedExecutesQueryOnFreePool) {
    BlockingSession session{make_test_config(), make_pool_config(2, 1)};

    auto outcome = session.with_sync(500ms);
    ASSERT_TRUE(outcome.is_success());
    auto exec = std::move(outcome).value();
    EXPECT_TRUE(exec.execute("SELECT 1").is_success());
}

TEST_F(BlockingSessionTest, WithSyncBlockingExecutesQueryOnFreePool) {
    BlockingSession session{make_test_config(), make_pool_config(2, 1)};

    auto outcome = session.with_sync();
    ASSERT_TRUE(outcome.is_success());
    auto exec = std::move(outcome).value();
    EXPECT_TRUE(exec.execute("SELECT 1").is_success());
}

// ============== Exhaustion + Error Codes ==============

TEST_F(BlockingSessionTest, TryWithSyncReturnsPoolExhaustedWhenFull) {
    BlockingSession session{make_test_config(), make_pool_config(1, 1)};

    auto first = session.try_with_sync();
    ASSERT_TRUE(first.is_success());

    auto second = session.try_with_sync();
    ASSERT_FALSE(second.is_success());
    EXPECT_EQ(second.error<ErrorContext>().code.value(), static_cast<int>(ClientErrorCode::PoolExhausted));
}

TEST_F(BlockingSessionTest, WithSyncTimedReturnsWaitTimeoutOnExpiry) {
    BlockingSession session{make_test_config(), make_pool_config(1, 1)};

    auto first = session.try_with_sync();
    ASSERT_TRUE(first.is_success());

    const auto start   = std::chrono::steady_clock::now();
    auto second        = session.with_sync(120ms);
    const auto elapsed = std::chrono::steady_clock::now() - start;

    ASSERT_FALSE(second.is_success());
    EXPECT_EQ(second.error<ErrorContext>().code.value(), static_cast<int>(ClientErrorCode::WaitTimeout));
    EXPECT_GE(elapsed, 110ms);
    EXPECT_LT(elapsed, 500ms);
}

// ============== Handoff on Release ==============

TEST_F(BlockingSessionTest, WithSyncTimedAcquiresSlotReleasedDuringWait) {
    BlockingSession session{make_test_config(), make_pool_config(1, 1)};

    auto first = session.try_with_sync();
    ASSERT_TRUE(first.is_success());

    auto holder_wrapper = std::make_unique<SyncExecutor>(std::move(first).value());

    std::thread releaser{[&] {
        std::this_thread::sleep_for(80ms);
        holder_wrapper.reset();  // dtor returns the slot
    }};

    const auto start = std::chrono::steady_clock::now();
    auto waited      = session.with_sync(2s);
    const auto took  = std::chrono::steady_clock::now() - start;

    releaser.join();

    ASSERT_TRUE(waited.is_success());
    EXPECT_GE(took, 70ms);
    EXPECT_LT(took, 500ms) << "handoff should be near-instant after release";
}

TEST_F(BlockingSessionTest, WithSyncBlockingAcquiresSlotReleasedDuringWait) {
    BlockingSession session{make_test_config(), make_pool_config(1, 1)};

    auto first          = session.try_with_sync();
    auto holder_wrapper = std::make_unique<SyncExecutor>(std::move(first).value());

    std::thread releaser{[&] {
        std::this_thread::sleep_for(80ms);
        holder_wrapper.reset();
    }};

    auto waited = session.with_sync();  // unbounded
    releaser.join();

    ASSERT_TRUE(waited.is_success());
}

// ============== Shutdown Semantics ==============

TEST_F(BlockingSessionTest, ShutdownWakesTimedWaiter) {
    BlockingSession session{make_test_config(), make_pool_config(1, 1)};
    auto first = session.try_with_sync();
    ASSERT_TRUE(first.is_success());

    std::atomic<bool> waiter_observed_shutdown{false};
    std::thread waiter{[&] {
        auto result              = session.with_sync(5s);
        waiter_observed_shutdown = !result.is_success() && result.error<ErrorContext>().code.value() ==
                                                               static_cast<int>(ClientErrorCode::PoolShutdown);
    }};

    // Wait until the waiter has enqueued
    while (session.pool_waiter_count() == 0) {
        std::this_thread::sleep_for(5ms);
    }

    session.shutdown();
    waiter.join();
    EXPECT_TRUE(waiter_observed_shutdown.load());
}

TEST_F(BlockingSessionTest, ShutdownWakesBlockingWaiter) {
    BlockingSession session{make_test_config(), make_pool_config(1, 1)};
    auto first = session.try_with_sync();
    ASSERT_TRUE(first.is_success());

    std::atomic<bool> waiter_observed_shutdown{false};
    std::thread waiter{[&] {
        auto result              = session.with_sync();  // unbounded
        waiter_observed_shutdown = !result.is_success() && result.error<ErrorContext>().code.value() ==
                                                               static_cast<int>(ClientErrorCode::PoolShutdown);
    }};

    while (session.pool_waiter_count() == 0) {
        std::this_thread::sleep_for(5ms);
    }

    session.shutdown();
    waiter.join();
    EXPECT_TRUE(waiter_observed_shutdown.load());
}

// ============== FIFO Fairness ==============

TEST_F(BlockingSessionTest, FifoFairnessUnderContention) {
    BlockingSession session{make_test_config(), make_pool_config(1, 1)};

    auto initial_outcome = session.try_with_sync();
    ASSERT_TRUE(initial_outcome.is_success());
    auto initial = std::make_unique<SyncExecutor>(std::move(initial_outcome).value());

    constexpr std::size_t N = 6;
    std::vector<std::thread> workers;
    std::vector<std::size_t> acquire_order;
    std::mutex order_mtx;

    // Launch workers one at a time, waiting for each to enqueue before starting the next.
    // This makes enqueue order deterministic — worker i is at waiters_[i].
    for (std::size_t i = 0; i < N; ++i) {
        workers.emplace_back([&, i] {
            if (const auto w = session.with_sync(10s); w.is_success()) {
                std::lock_guard lk{order_mtx};
                acquire_order.push_back(i);
                // Hold briefly so subsequent waiters don't race past us
                std::this_thread::sleep_for(30ms);
            }
        });

        // Wait until this worker has enqueued before launching the next
        while (session.pool_waiter_count() != i + 1) {
            std::this_thread::sleep_for(2ms);
        }
    }

    // All N workers are enqueued. Release the initial holder — the first worker wakes.
    initial.reset();

    for (auto& t : workers)
        t.join();

    ASSERT_EQ(acquire_order.size(), static_cast<std::size_t>(N));
    for (std::size_t i = 0; i < static_cast<std::size_t>(N); ++i) {
        EXPECT_EQ(acquire_order[i], static_cast<int>(i))
            << "FIFO order violated at position " << i << " (got worker " << acquire_order[i] << ")";
    }
}

// ============== Transactions ==============

TEST_F(BlockingSessionTest, BeginTransactionRollsBackOnScopeExit) {
    BlockingSession session{make_test_config(), make_pool_config(2, 1)};

    // Setup: create a test table in an auto-committed statement
    {
        auto exec_outcome = session.try_with_sync();
        ASSERT_TRUE(exec_outcome.is_success());
        auto exec = std::move(exec_outcome).value();
        ASSERT_TRUE(exec.execute("CREATE TABLE IF NOT EXISTS blocking_tx_test (id INT)").is_success());
        ASSERT_TRUE(exec.execute("TRUNCATE blocking_tx_test").is_success());
    }

    // Insert inside a transaction and roll back
    {
        auto tx_outcome = session.try_begin_transaction();
        ASSERT_TRUE(tx_outcome.is_success());
        auto tx = std::move(tx_outcome).value();
        ASSERT_TRUE(tx.begin().is_success());
        {
            auto exec_outcome = tx.with_sync();
            ASSERT_TRUE(exec_outcome.is_success());
            auto exec = std::move(exec_outcome).value();
            ASSERT_TRUE(exec.execute("INSERT INTO blocking_tx_test VALUES (1)").is_success());
        }
        ASSERT_TRUE(tx.rollback().is_success());
    }

    // Verify the row is gone
    {
        auto exec_outcome = session.try_with_sync();
        ASSERT_TRUE(exec_outcome.is_success());
        auto exec   = std::move(exec_outcome).value();
        auto result = exec.execute("SELECT COUNT(*) FROM blocking_tx_test");
        ASSERT_TRUE(result.is_success());
        EXPECT_EQ(result.value().rows(), 1u);
    }

    // Cleanup
    {
        auto exec_outcome = session.try_with_sync();
        ASSERT_TRUE(exec_outcome.is_success());
        auto exec = std::move(exec_outcome).value();
        (void)exec.execute("DROP TABLE IF EXISTS blocking_tx_test");
    }
}

TEST_F(BlockingSessionTest, TryBeginAutoTransactionAutoCommits) {
    BlockingSession session{make_test_config(), make_pool_config(2, 1)};

    {
        auto exec_outcome = session.try_with_sync();
        ASSERT_TRUE(exec_outcome.is_success());
        auto exec = std::move(exec_outcome).value();
        ASSERT_TRUE(exec.execute("CREATE TABLE IF NOT EXISTS blocking_autotx_test (id INT)").is_success());
        ASSERT_TRUE(exec.execute("TRUNCATE blocking_autotx_test").is_success());
    }

    {
        auto tx_outcome = session.try_begin_auto_transaction();
        ASSERT_TRUE(tx_outcome.is_success());
        auto tx = std::move(tx_outcome).value();
        {
            auto exec_outcome = tx.with_sync();
            ASSERT_TRUE(exec_outcome.is_success());
            auto exec = std::move(exec_outcome).value();
            ASSERT_TRUE(exec.execute("INSERT INTO blocking_autotx_test VALUES (7)").is_success());
        }
        ASSERT_TRUE(tx.commit().is_success());
    }

    {
        auto exec_outcome = session.try_with_sync();
        ASSERT_TRUE(exec_outcome.is_success());
        auto exec   = std::move(exec_outcome).value();
        auto result = exec.execute("SELECT id FROM blocking_autotx_test");
        ASSERT_TRUE(result.is_success());
        EXPECT_EQ(result.value().rows(), 1u);

        (void)exec.execute("DROP TABLE IF EXISTS blocking_autotx_test");
    }
}

// ============== Stats ==============

TEST_F(BlockingSessionTest, StatsReflectFreeAndActiveHolders) {
    BlockingSession session{make_test_config(), make_pool_config(4, 2)};

    EXPECT_EQ(session.pool_capacity(), 4u);
    EXPECT_EQ(session.pool_free_count(), 2u);
    EXPECT_EQ(session.pool_active_count(), 0u);

    auto a = session.try_with_sync();
    ASSERT_TRUE(a.is_success());
    EXPECT_EQ(session.pool_active_count(), 1u);
    EXPECT_EQ(session.pool_free_count(), 1u);

    auto b = session.try_with_sync();
    ASSERT_TRUE(b.is_success());
    EXPECT_EQ(session.pool_active_count(), 2u);
    EXPECT_EQ(session.pool_free_count(), 0u);

    // Lazy init creates a 3rd
    auto c = session.try_with_sync();
    ASSERT_TRUE(c.is_success());
    EXPECT_EQ(session.pool_active_count(), 3u);
    EXPECT_EQ(session.pool_free_count(), 0u);
}
