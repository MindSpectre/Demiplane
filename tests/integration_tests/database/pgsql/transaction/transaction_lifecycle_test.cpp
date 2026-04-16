// PostgreSQL Transaction Lifecycle Integration Tests
// Tests Transaction begin/commit/rollback, status transitions, and AutoTransaction

#include <boost/asio.hpp>
#include <gtest/gtest.h>
#include <postgres_session.hpp>

using namespace demiplane::db::postgres;
using namespace demiplane::db;
using namespace std::chrono_literals;

// TODO: Extract make_test_config() into a shared test utility — duplicated in savepoint_test.cpp.

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

// ============== Transaction Lifecycle Tests ==============

class TransactionLifecycleTest : public ::testing::Test {
protected:
    void SetUp() override {
        const auto conn_string = make_test_config().to_connection_string();
        PGconn* probe          = PQconnectdb(conn_string.c_str());

        if (!probe || PQstatus(probe) != CONNECTION_OK) {
            const std::string error = probe ? PQerrorMessage(probe) : "null connection";
            if (probe)
                PQfinish(probe);
            GTEST_SKIP() << "PostgreSQL unavailable: " << error;
        }
        PQfinish(probe);

        session_ = std::make_unique<LockFreeSession>(
            make_test_config(),
            PoolConfig::Builder{}.capacity(4).min_connections(1).health_check_interval(2s).finalize());

        // Create test table
        auto exec   = session_->with_sync().value();
        auto result = exec.execute(R"(
            CREATE TABLE IF NOT EXISTS tx_test (
                id SERIAL PRIMARY KEY,
                name VARCHAR(100) NOT NULL
            )
        )");
        ASSERT_TRUE(result.is_success()) << "Setup failed: " << result.error<ErrorContext>().format();

        auto truncate = exec.execute("TRUNCATE TABLE tx_test RESTART IDENTITY CASCADE");
        ASSERT_TRUE(truncate.is_success()) << "Truncate failed: " << truncate.error<ErrorContext>().format();
    }

    void TearDown() override {
        if (session_) {
            const auto exec  = session_->with_sync().value();
            GEARS_UNUSED_VAR = exec.execute("DROP TABLE IF EXISTS tx_test CASCADE");
            session_->shutdown();
        }
    }

    std::unique_ptr<LockFreeSession> session_;
};

// ============== Manual Transaction Tests ==============

TEST_F(TransactionLifecycleTest, BeginCommitPersistsData) {
    // Begin transaction, insert, commit
    auto tx_result = session_->begin_transaction();
    ASSERT_TRUE(tx_result.is_success()) << tx_result.error<ErrorContext>().format();
    auto tx = std::move(tx_result.value());

    ASSERT_TRUE(tx.begin().is_success());

    auto insert = tx.with_sync().value().execute("INSERT INTO tx_test (name) VALUES ('committed_row')");
    ASSERT_TRUE(insert.is_success()) << insert.error<ErrorContext>().format();

    ASSERT_TRUE(tx.commit().is_success());

    // Verify data persisted via session
    auto exec   = session_->with_sync().value();
    auto select = exec.execute("SELECT name FROM tx_test WHERE name = 'committed_row'");
    ASSERT_TRUE(select.is_success()) << select.error<ErrorContext>().format();
    EXPECT_EQ(select.value().rows(), 1);
    EXPECT_EQ(select.value().get<std::string>(0, 0), "committed_row");
}

TEST_F(TransactionLifecycleTest, BeginRollbackDiscardsData) {
    // Begin transaction, insert, rollback
    auto tx_result = session_->begin_transaction();
    ASSERT_TRUE(tx_result.is_success()) << tx_result.error<ErrorContext>().format();
    auto tx = std::move(tx_result.value());

    ASSERT_TRUE(tx.begin().is_success());

    auto insert = tx.with_sync().value().execute("INSERT INTO tx_test (name) VALUES ('rolled_back_row')");
    ASSERT_TRUE(insert.is_success()) << insert.error<ErrorContext>().format();

    ASSERT_TRUE(tx.rollback().is_success());

    // Verify data is absent
    auto exec   = session_->with_sync().value();
    auto select = exec.execute("SELECT name FROM tx_test WHERE name = 'rolled_back_row'");
    ASSERT_TRUE(select.is_success()) << select.error<ErrorContext>().format();
    EXPECT_EQ(select.value().rows(), 0);
}

TEST_F(TransactionLifecycleTest, DestructorReleasesSlotWithoutCommit) {
    const auto free_before = session_->pool_free_count();

    {
        auto tx_result = session_->begin_transaction();
        ASSERT_TRUE(tx_result.is_success()) << tx_result.error<ErrorContext>().format();
        auto tx = std::move(tx_result.value());

        ASSERT_TRUE(tx.begin().is_success());

        auto insert = tx.with_sync().value().execute("INSERT INTO tx_test (name) VALUES ('orphaned_row')");
        ASSERT_TRUE(insert.is_success()) << insert.error<ErrorContext>().format();

        // tx destroyed here without commit or explicit rollback
    }

    // Slot should be freed back to pool
    EXPECT_GE(session_->pool_free_count(), free_before);

    // Data should not persist (connection reset via DISCARD ALL or implicit rollback)
    auto exec   = session_->with_sync().value();
    auto select = exec.execute("SELECT name FROM tx_test WHERE name = 'orphaned_row'");
    ASSERT_TRUE(select.is_success()) << select.error<ErrorContext>().format();
    EXPECT_EQ(select.value().rows(), 0);
}

TEST_F(TransactionLifecycleTest, StatusStartsAsIdle) {
    auto tx_result = session_->begin_transaction();
    ASSERT_TRUE(tx_result.is_success()) << tx_result.error<ErrorContext>().format();
    auto tx = std::move(tx_result.value());

    EXPECT_EQ(tx.status(), TransactionStatus::IDLE);
    EXPECT_FALSE(tx.is_active());
    EXPECT_FALSE(tx.is_finished());
}

TEST_F(TransactionLifecycleTest, CommitWithoutBeginReturnsError) {
    auto tx_result = session_->begin_transaction();
    ASSERT_TRUE(tx_result.is_success()) << tx_result.error<ErrorContext>().format();
    auto tx = std::move(tx_result.value());

    // Commit on IDLE transaction should fail
    auto result = tx.commit();
    ASSERT_FALSE(result.is_success());
    EXPECT_EQ(result.error<ErrorContext>().code, ClientErrorCode::InvalidState);
}

TEST_F(TransactionLifecycleTest, RollbackWithoutBeginReturnsError) {
    auto tx_result = session_->begin_transaction();
    ASSERT_TRUE(tx_result.is_success()) << tx_result.error<ErrorContext>().format();
    auto tx = std::move(tx_result.value());

    // Rollback on IDLE transaction should fail
    auto result = tx.rollback();
    ASSERT_FALSE(result.is_success());
    EXPECT_EQ(result.error<ErrorContext>().code, ClientErrorCode::InvalidState);
}

TEST_F(TransactionLifecycleTest, DoubleCommitReturnsError) {
    auto tx_result = session_->begin_transaction();
    ASSERT_TRUE(tx_result.is_success()) << tx_result.error<ErrorContext>().format();
    auto tx = std::move(tx_result.value());

    ASSERT_TRUE(tx.begin().is_success());
    ASSERT_TRUE(tx.commit().is_success());

    // Second commit should fail
    auto result = tx.commit();
    ASSERT_FALSE(result.is_success());
    EXPECT_EQ(result.error<ErrorContext>().code, ClientErrorCode::InvalidState);
}

TEST_F(TransactionLifecycleTest, WithSyncOnIdleTransactionReturnsInvalidExecutor) {
    auto tx_result = session_->begin_transaction();
    ASSERT_TRUE(tx_result.is_success()) << tx_result.error<ErrorContext>().format();
    const auto tx = std::move(tx_result.value());

    // Transaction is IDLE, with_sync should return error
    const auto result = tx.with_sync();
    EXPECT_FALSE(result.is_success());
}

TEST_F(TransactionLifecycleTest, WithSyncOnActiveTransactionReturnsValidExecutor) {
    auto tx_result = session_->begin_transaction();
    ASSERT_TRUE(tx_result.is_success()) << tx_result.error<ErrorContext>().format();
    auto tx = std::move(tx_result.value());

    ASSERT_TRUE(tx.begin().is_success());

    const auto result = tx.with_sync();
    EXPECT_TRUE(result.is_success());
}

TEST_F(TransactionLifecycleTest, MultipleQueriesInSameTransaction) {
    auto tx_result = session_->begin_transaction();
    ASSERT_TRUE(tx_result.is_success()) << tx_result.error<ErrorContext>().format();
    auto tx = std::move(tx_result.value());

    ASSERT_TRUE(tx.begin().is_success());

    // Insert 3 rows
    for (int i = 1; i <= 3; ++i) {
        auto insert = tx.with_sync().value().execute("INSERT INTO tx_test (name) VALUES ($1)",
                                                     std::string{"row_" + std::to_string(i)});
        ASSERT_TRUE(insert.is_success()) << "Insert " << i << " failed: " << insert.error<ErrorContext>().format();
    }

    // Verify count within transaction
    auto count = tx.with_sync().value().execute("SELECT COUNT(*) FROM tx_test");
    ASSERT_TRUE(count.is_success()) << count.error<ErrorContext>().format();
    EXPECT_EQ(count.value().get<int>(0, 0), 3);

    ASSERT_TRUE(tx.commit().is_success());

    // Verify count after commit via session
    auto exec   = session_->with_sync().value();
    auto select = exec.execute("SELECT COUNT(*) FROM tx_test");
    ASSERT_TRUE(select.is_success()) << select.error<ErrorContext>().format();
    EXPECT_EQ(select.value().get<int>(0, 0), 3);
}

// TODO: Add with_async() tests — currently only with_sync() executor paths are covered.
//       Verify with_async() returns valid/invalid executor under same state conditions.

// ============== AutoTransaction Tests ==============

TEST_F(TransactionLifecycleTest, AutoTransactionIsActiveImmediately) {
    auto auto_result = session_->begin_auto_transaction();
    ASSERT_TRUE(auto_result.is_success()) << auto_result.error<ErrorContext>().format();
    auto atx = std::move(auto_result.value());

    EXPECT_EQ(atx.status(), TransactionStatus::ACTIVE);
    EXPECT_TRUE(atx.is_active());
    EXPECT_FALSE(atx.is_finished());
}

TEST_F(TransactionLifecycleTest, AutoTransactionCommitPersistsData) {
    auto auto_result = session_->begin_auto_transaction();
    ASSERT_TRUE(auto_result.is_success()) << auto_result.error<ErrorContext>().format();
    auto atx = std::move(auto_result.value());

    auto insert = atx.with_sync().value().execute("INSERT INTO tx_test (name) VALUES ('auto_committed')");
    ASSERT_TRUE(insert.is_success()) << insert.error<ErrorContext>().format();

    ASSERT_TRUE(atx.commit().is_success());

    // Verify data persisted
    auto exec   = session_->with_sync().value();
    auto select = exec.execute("SELECT name FROM tx_test WHERE name = 'auto_committed'");
    ASSERT_TRUE(select.is_success()) << select.error<ErrorContext>().format();
    EXPECT_EQ(select.value().rows(), 1);
    EXPECT_EQ(select.value().get<std::string>(0, 0), "auto_committed");
}

TEST_F(TransactionLifecycleTest, AutoTransactionDestructorImplicitRollback) {
    {
        auto auto_result = session_->begin_auto_transaction();
        ASSERT_TRUE(auto_result.is_success()) << auto_result.error<ErrorContext>().format();
        auto atx = std::move(auto_result.value());

        auto insert = atx.with_sync().value().execute("INSERT INTO tx_test (name) VALUES ('auto_orphaned')");
        ASSERT_TRUE(insert.is_success()) << insert.error<ErrorContext>().format();

        // atx destroyed here without commit -- implicit rollback via slot reset
    }

    // Verify data is absent
    auto exec   = session_->with_sync().value();
    auto select = exec.execute("SELECT name FROM tx_test WHERE name = 'auto_orphaned'");
    ASSERT_TRUE(select.is_success()) << select.error<ErrorContext>().format();
    EXPECT_EQ(select.value().rows(), 0);
}
