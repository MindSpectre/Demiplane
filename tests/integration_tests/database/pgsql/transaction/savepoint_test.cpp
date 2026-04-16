// PostgreSQL Savepoint Integration Tests
// Tests savepoint creation, rollback, release, nesting, and error handling

#include <base/transaction.hpp>
#include <boost/asio.hpp>
#include <gtest/gtest.h>
#include <postgres_session.hpp>
#include <savepoint/savepoint.hpp>

using namespace demiplane::db::postgres;
using namespace demiplane::db;
using namespace std::chrono_literals;

// TODO: Extract make_test_config() into a shared test utility — duplicated in transaction_lifecycle_test.cpp.

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

// ============== Savepoint Tests ==============

class SavepointTest : public ::testing::Test {
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
            CREATE TABLE IF NOT EXISTS sp_test (
                id SERIAL PRIMARY KEY,
                name VARCHAR(100) NOT NULL
            )
        )");
        ASSERT_TRUE(result.is_success()) << "Setup failed: " << result.error<ErrorContext>().format();

        auto truncate = exec.execute("TRUNCATE TABLE sp_test RESTART IDENTITY CASCADE");
        ASSERT_TRUE(truncate.is_success()) << "Truncate failed: " << truncate.error<ErrorContext>().format();
    }

    void TearDown() override {
        if (session_) {
            const auto exec  = session_->with_sync().value();
            GEARS_UNUSED_VAR = exec.execute("DROP TABLE IF EXISTS sp_test CASCADE");
            session_->shutdown();
        }
    }

    std::unique_ptr<LockFreeSession> session_;
};

TEST_F(SavepointTest, RollbackToSavepointUndoesWork) {
    auto tx_result = session_->begin_transaction();
    ASSERT_TRUE(tx_result.is_success()) << tx_result.error<ErrorContext>().format();
    auto tx = std::move(tx_result.value());

    ASSERT_TRUE(tx.begin().is_success());

    // Insert 'Before' before savepoint
    auto insert_before = tx.with_sync().value().execute("INSERT INTO sp_test (name) VALUES ('Before')");
    ASSERT_TRUE(insert_before.is_success()) << insert_before.error<ErrorContext>().format();

    // Create savepoint
    auto sp_result = tx.savepoint("sp1");
    ASSERT_TRUE(sp_result.is_success()) << sp_result.error<ErrorContext>().format();
    auto sp = std::move(sp_result.value());

    // Insert 'After' after savepoint
    auto insert_after = tx.with_sync().value().execute("INSERT INTO sp_test (name) VALUES ('After')");
    ASSERT_TRUE(insert_after.is_success()) << insert_after.error<ErrorContext>().format();

    // Verify both rows visible within transaction
    auto count_before_rb = tx.with_sync().value().execute("SELECT COUNT(*) FROM sp_test");
    ASSERT_TRUE(count_before_rb.is_success()) << count_before_rb.error<ErrorContext>().format();
    EXPECT_EQ(count_before_rb.value().get<int>(0, 0), 2);

    // Rollback to savepoint -- undoes 'After'
    ASSERT_TRUE(sp.rollback().is_success());

    // Verify only 'Before' remains within transaction
    auto select = tx.with_sync().value().execute("SELECT name FROM sp_test");
    ASSERT_TRUE(select.is_success()) << select.error<ErrorContext>().format();
    EXPECT_EQ(select.value().rows(), 1);
    EXPECT_EQ(select.value().get<std::string>(0, 0), "Before");

    // Commit and verify final state
    ASSERT_TRUE(tx.commit().is_success());

    auto exec        = session_->with_sync().value();
    auto final_check = exec.execute("SELECT name FROM sp_test");
    ASSERT_TRUE(final_check.is_success()) << final_check.error<ErrorContext>().format();
    EXPECT_EQ(final_check.value().rows(), 1);
    EXPECT_EQ(final_check.value().get<std::string>(0, 0), "Before");
}

TEST_F(SavepointTest, ReleaseSavepointKeepsWork) {
    auto tx_result = session_->begin_transaction();
    ASSERT_TRUE(tx_result.is_success()) << tx_result.error<ErrorContext>().format();
    auto tx = std::move(tx_result.value());

    ASSERT_TRUE(tx.begin().is_success());

    // Insert 'Keep'
    auto insert1 = tx.with_sync().value().execute("INSERT INTO sp_test (name) VALUES ('Keep')");
    ASSERT_TRUE(insert1.is_success()) << insert1.error<ErrorContext>().format();

    // Create savepoint
    auto sp_result = tx.savepoint("sp_keep");
    ASSERT_TRUE(sp_result.is_success()) << sp_result.error<ErrorContext>().format();
    auto sp = std::move(sp_result.value());

    // Insert 'AlsoKeep'
    auto insert2 = tx.with_sync().value().execute("INSERT INTO sp_test (name) VALUES ('AlsoKeep')");
    ASSERT_TRUE(insert2.is_success()) << insert2.error<ErrorContext>().format();

    // Release savepoint -- keeps all work
    ASSERT_TRUE(sp.release().is_success());

    // Commit
    ASSERT_TRUE(tx.commit().is_success());

    // Verify both rows persisted
    auto exec   = session_->with_sync().value();
    auto select = exec.execute("SELECT COUNT(*) FROM sp_test");
    ASSERT_TRUE(select.is_success()) << select.error<ErrorContext>().format();
    EXPECT_EQ(select.value().get<int>(0, 0), 2);
}

TEST_F(SavepointTest, NestedSavepoints) {
    auto tx_result = session_->begin_transaction();
    ASSERT_TRUE(tx_result.is_success()) << tx_result.error<ErrorContext>().format();
    auto tx = std::move(tx_result.value());

    ASSERT_TRUE(tx.begin().is_success());

    // Insert 'Base'
    auto insert_base = tx.with_sync().value().execute("INSERT INTO sp_test (name) VALUES ('Base')");
    ASSERT_TRUE(insert_base.is_success()) << insert_base.error<ErrorContext>().format();

    // Outer savepoint
    auto outer_result = tx.savepoint("outer");
    ASSERT_TRUE(outer_result.is_success()) << outer_result.error<ErrorContext>().format();
    auto outer = std::move(outer_result.value());

    // Insert 'Middle'
    auto insert_middle = tx.with_sync().value().execute("INSERT INTO sp_test (name) VALUES ('Middle')");
    ASSERT_TRUE(insert_middle.is_success()) << insert_middle.error<ErrorContext>().format();

    // Inner savepoint
    auto inner_result = tx.savepoint("inner");
    ASSERT_TRUE(inner_result.is_success()) << inner_result.error<ErrorContext>().format();
    auto inner = std::move(inner_result.value());

    // Insert 'Deep'
    auto insert_deep = tx.with_sync().value().execute("INSERT INTO sp_test (name) VALUES ('Deep')");
    ASSERT_TRUE(insert_deep.is_success()) << insert_deep.error<ErrorContext>().format();

    // Rollback inner -- undoes 'Deep'
    ASSERT_TRUE(inner.rollback().is_success());

    // Release outer -- keeps 'Base' and 'Middle'
    ASSERT_TRUE(outer.release().is_success());

    // Commit
    ASSERT_TRUE(tx.commit().is_success());

    // Verify 2 rows: Base and Middle
    auto exec   = session_->with_sync().value();
    auto select = exec.execute("SELECT name FROM sp_test ORDER BY id");
    ASSERT_TRUE(select.is_success()) << select.error<ErrorContext>().format();
    EXPECT_EQ(select.value().rows(), 2);
    EXPECT_EQ(select.value().get<std::string>(0, 0), "Base");
    EXPECT_EQ(select.value().get<std::string>(1, 0), "Middle");
}

TEST_F(SavepointTest, SavepointOnNonActiveTransactionReturnsError) {
    auto tx_result = session_->begin_transaction();
    ASSERT_TRUE(tx_result.is_success()) << tx_result.error<ErrorContext>().format();
    auto tx = std::move(tx_result.value());

    // Transaction is IDLE (begin() not called), savepoint should fail
    auto sp_result = tx.savepoint("should_fail");
    ASSERT_FALSE(sp_result.is_success());
    EXPECT_EQ(sp_result.error<ErrorContext>().code, ClientErrorCode::InvalidState);
}

TEST_F(SavepointTest, DoubleRollbackSucceeds) {
    auto tx_result = session_->begin_transaction();
    ASSERT_TRUE(tx_result.is_success()) << tx_result.error<ErrorContext>().format();
    auto tx = std::move(tx_result.value());

    ASSERT_TRUE(tx.begin().is_success());

    auto sp_result = tx.savepoint("sp_double");
    ASSERT_TRUE(sp_result.is_success()) << sp_result.error<ErrorContext>().format();
    auto sp = std::move(sp_result.value());

    // First rollback succeeds, savepoint stays active (PostgreSQL semantics)
    ASSERT_TRUE(sp.rollback().is_success());
    EXPECT_TRUE(sp.is_active());

    // Second rollback also succeeds
    ASSERT_TRUE(sp.rollback().is_success());
    EXPECT_TRUE(sp.is_active());

    // Cleanup: release the savepoint
    ASSERT_TRUE(sp.release().is_success());
    EXPECT_FALSE(sp.is_active());
}

TEST_F(SavepointTest, RollbackRetrySemantics) {
    auto tx_result = session_->begin_transaction();
    ASSERT_TRUE(tx_result.is_success()) << tx_result.error<ErrorContext>().format();
    auto tx = std::move(tx_result.value());

    ASSERT_TRUE(tx.begin().is_success());

    // Insert pre-savepoint data
    auto insert_base = tx.with_sync().value().execute("INSERT INTO sp_test (name) VALUES ('base')");
    ASSERT_TRUE(insert_base.is_success()) << insert_base.error<ErrorContext>().format();

    // Create savepoint
    auto sp_result = tx.savepoint("sp_retry");
    ASSERT_TRUE(sp_result.is_success()) << sp_result.error<ErrorContext>().format();
    auto sp = std::move(sp_result.value());

    // First attempt: insert and rollback
    auto attempt1 = tx.with_sync().value().execute("INSERT INTO sp_test (name) VALUES ('attempt1')");
    ASSERT_TRUE(attempt1.is_success()) << attempt1.error<ErrorContext>().format();
    ASSERT_TRUE(sp.rollback().is_success());
    EXPECT_TRUE(sp.is_active());

    // Second attempt: insert and rollback again
    auto attempt2 = tx.with_sync().value().execute("INSERT INTO sp_test (name) VALUES ('attempt2')");
    ASSERT_TRUE(attempt2.is_success()) << attempt2.error<ErrorContext>().format();
    ASSERT_TRUE(sp.rollback().is_success());
    EXPECT_TRUE(sp.is_active());

    // Release savepoint and commit
    ASSERT_TRUE(sp.release().is_success());
    ASSERT_TRUE(tx.commit().is_success());

    // Only 'base' should remain — both attempts were rolled back
    auto exec   = session_->with_sync().value();
    auto select = exec.execute("SELECT name FROM sp_test ORDER BY id");
    ASSERT_TRUE(select.is_success()) << select.error<ErrorContext>().format();
    EXPECT_EQ(select.value().rows(), 1);
    EXPECT_EQ(select.value().get<std::string>(0, 0), "base");
}

TEST_F(SavepointTest, DestructorReleasesSavepointIfActive) {
    auto tx_result = session_->begin_transaction();
    ASSERT_TRUE(tx_result.is_success()) << tx_result.error<ErrorContext>().format();
    auto tx = std::move(tx_result.value());

    ASSERT_TRUE(tx.begin().is_success());

    // Insert before savepoint
    auto insert1 = tx.with_sync().value().execute("INSERT INTO sp_test (name) VALUES ('before_scope')");
    ASSERT_TRUE(insert1.is_success()) << insert1.error<ErrorContext>().format();

    {
        // Create savepoint in inner scope
        auto sp_result = tx.savepoint("sp_scoped");
        ASSERT_TRUE(sp_result.is_success()) << sp_result.error<ErrorContext>().format();
        auto sp = std::move(sp_result.value());
        EXPECT_TRUE(sp.is_active());

        // sp destroyed here -- destructor should release the savepoint
    }

    // Insert after savepoint scope ended
    auto insert2 = tx.with_sync().value().execute("INSERT INTO sp_test (name) VALUES ('after_scope')");
    ASSERT_TRUE(insert2.is_success()) << insert2.error<ErrorContext>().format();

    // Commit
    ASSERT_TRUE(tx.commit().is_success());

    // Verify both rows persisted
    auto exec   = session_->with_sync().value();
    auto select = exec.execute("SELECT name FROM sp_test ORDER BY id");
    ASSERT_TRUE(select.is_success()) << select.error<ErrorContext>().format();
    EXPECT_EQ(select.value().rows(), 2);
    EXPECT_EQ(select.value().get<std::string>(0, 0), "before_scope");
    EXPECT_EQ(select.value().get<std::string>(1, 0), "after_scope");
}
