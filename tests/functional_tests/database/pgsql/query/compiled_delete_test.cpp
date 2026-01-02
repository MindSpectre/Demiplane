// Compiled DELETE Query Functional Tests
// Tests query compilation + execution with SyncExecutor using QueryLibrary

#include <demiplane/nexus>
#include <demiplane/scroll>

#include <gtest/gtest.h>

#include "postgres_dialect.hpp"
#include "postgres_params.hpp"
#include "postgres_sync_executor.hpp"
#include "query_library.hpp"

using namespace demiplane::db;
using namespace demiplane::db::postgres;
using namespace demiplane::test;

// Test fixture for compiled DELETE queries
class CompiledDeleteTest : public ::testing::Test {
protected:
    void SetUp() override {
        demiplane::nexus::instance()
            .register_singleton<demiplane::scroll::ConsoleSink<demiplane::scroll::DetailedEntry>>([] {
                return std::make_shared<demiplane::scroll::ConsoleSink<demiplane::scroll::DetailedEntry>>(
                    demiplane::scroll::ConsoleSinkConfig{}
                        .flush_each_entry(true)
                        .threshold(demiplane::scroll::TRC)
                        .finalize());
            });

        demiplane::nexus::instance().register_singleton<demiplane::scroll::Logger>([] {
            auto logger = std::make_shared<demiplane::scroll::Logger>();
            logger->add_sink(
                demiplane::nexus::instance().get<demiplane::scroll::ConsoleSink<demiplane::scroll::DetailedEntry>>());
            return logger;
        });

        const char* host     = std::getenv("POSTGRES_HOST") ? std::getenv("POSTGRES_HOST") : "localhost";
        const char* port     = std::getenv("POSTGRES_PORT") ? std::getenv("POSTGRES_PORT") : "5433";
        const char* dbname   = std::getenv("POSTGRES_DB") ? std::getenv("POSTGRES_DB") : "test_db";
        const char* user     = std::getenv("POSTGRES_USER") ? std::getenv("POSTGRES_USER") : "test_user";
        const char* password = std::getenv("POSTGRES_PASSWORD") ? std::getenv("POSTGRES_PASSWORD") : "test_password";

        std::string conninfo = "host=" + std::string(host) + " port=" + std::string(port) +
                               " dbname=" + std::string(dbname) + " user=" + std::string(user) +
                               " password=" + std::string(password);

        conn_ = PQconnectdb(conninfo.c_str());

        if (PQstatus(conn_) != CONNECTION_OK) {
            std::string error = PQerrorMessage(conn_);
            PQfinish(conn_);
            conn_ = nullptr;
            GTEST_SKIP() << "Failed to connect to PostgreSQL: " << error;
        }

        executor_ = std::make_unique<SyncExecutor>(conn_);
        library_  = std::make_unique<QueryLibrary>(std::make_unique<Dialect>());

        CreateTable();
    }

    void TearDown() override {
        if (conn_) {
            DropTable();
            PQfinish(conn_);
            conn_ = nullptr;
        }
    }

    void CreateTable() {
        auto create_result = executor_->execute(R"(
            CREATE TABLE IF NOT EXISTS users (
                id SERIAL PRIMARY KEY,
                name VARCHAR(100),
                age INTEGER,
                active BOOLEAN
            )
        )");
        ASSERT_TRUE(create_result.is_success())
            << "Failed to create table: " << create_result.error<ErrorContext>();
        CleanTable();
    }

    void DropTable() {
        (void)executor_->execute("DROP TABLE IF EXISTS users CASCADE");
    }

    void CleanTable() const {
        auto result = executor_->execute("TRUNCATE TABLE users RESTART IDENTITY CASCADE");
        ASSERT_TRUE(result.is_success()) << "Failed to clean table: " << result.error<ErrorContext>();
    }

    [[nodiscard]] int CountRows() const {
        auto result = executor_->execute("SELECT COUNT(*) FROM users");
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
    std::unique_ptr<QueryLibrary> library_;
};

// ============== Basic DELETE Tests ==============

TEST_F(CompiledDeleteTest, DeleteSingleRow) {
    EXPECT_TRUE(executor_->execute("INSERT INTO users (name, age, active) VALUES ('Alice', 30, true)"));
    EXPECT_TRUE(executor_->execute("INSERT INTO users (name, age, active) VALUES ('Bob', 25, false)"));

    const auto& s = library_->schemas().users();
    auto query          = delete_from(s.table).where(s.name == std::string{"Alice"});
    auto compiled_query = library_->compiler().compile(query);

    auto result = executor_->execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Delete failed: " << result.error<ErrorContext>();

    EXPECT_EQ(CountRows(), 1);  // Only Bob should remain

    auto select_result = executor_->execute("SELECT COUNT(*) FROM users WHERE name = 'Alice'");
    ASSERT_TRUE(select_result.is_success());
    EXPECT_EQ(select_result.value().get<int>(0, 0), 0);
}

TEST_F(CompiledDeleteTest, DeleteMultipleRows) {
    EXPECT_TRUE(executor_->execute("INSERT INTO users (name, age, active) VALUES ('User1', 20, false)"));
    EXPECT_TRUE(executor_->execute("INSERT INTO users (name, age, active) VALUES ('User2', 30, false)"));
    EXPECT_TRUE(executor_->execute("INSERT INTO users (name, age, active) VALUES ('User3', 40, true)"));

    auto query = library_->produce<del::BasicDelete>();
    auto result = executor_->execute(query);

    ASSERT_TRUE(result.is_success()) << "Delete failed: " << result.error<ErrorContext>();

    EXPECT_EQ(CountRows(), 1);  // Only User3 should remain

    auto select_result = executor_->execute("SELECT COUNT(*) FROM users WHERE active = true");
    ASSERT_TRUE(select_result.is_success());
    EXPECT_EQ(select_result.value().get<int>(0, 0), 1);
}

// ============== DELETE with WHERE Conditions ==============

TEST_F(CompiledDeleteTest, DeleteWithSimpleWhere) {
    EXPECT_TRUE(executor_->execute("INSERT INTO users (name, age) VALUES ('User1', 20)"));
    EXPECT_TRUE(executor_->execute("INSERT INTO users (name, age) VALUES ('User2', 30)"));
    EXPECT_TRUE(executor_->execute("INSERT INTO users (name, age) VALUES ('User3', 40)"));

    const auto& s = library_->schemas().users();
    auto query          = delete_from(s.table).where(s.age > 25);
    auto compiled_query = library_->compiler().compile(query);

    auto result = executor_->execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Delete failed: " << result.error<ErrorContext>();

    EXPECT_EQ(CountRows(), 1);  // Only User1 should remain
}

TEST_F(CompiledDeleteTest, DeleteWithComplexWhere) {
    EXPECT_TRUE(executor_->execute("INSERT INTO users (name, age, active) VALUES ('User1', 25, true)"));
    EXPECT_TRUE(executor_->execute("INSERT INTO users (name, age, active) VALUES ('User2', 30, true)"));
    EXPECT_TRUE(executor_->execute("INSERT INTO users (name, age, active) VALUES ('User3', 35, false)"));
    EXPECT_TRUE(executor_->execute("INSERT INTO users (name, age, active) VALUES ('User4', 40, true)"));

    const auto& s = library_->schemas().users();
    auto query          = delete_from(s.table).where((s.age >= 30) && (s.active == true));
    auto compiled_query = library_->compiler().compile(query);

    auto result = executor_->execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Delete failed: " << result.error<ErrorContext>();

    EXPECT_EQ(CountRows(), 2);  // User1 and User3 should remain
}

TEST_F(CompiledDeleteTest, DeleteWithOrCondition) {
    EXPECT_TRUE(executor_->execute("INSERT INTO users (name, age) VALUES ('User1', 20)"));
    EXPECT_TRUE(executor_->execute("INSERT INTO users (name, age) VALUES ('User2', 30)"));
    EXPECT_TRUE(executor_->execute("INSERT INTO users (name, age) VALUES ('User3', 40)"));

    const auto& s = library_->schemas().users();
    auto query          = delete_from(s.table).where((s.age < 25) || (s.age > 35));
    auto compiled_query = library_->compiler().compile(query);

    auto result = executor_->execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Delete failed: " << result.error<ErrorContext>();

    EXPECT_EQ(CountRows(), 1);  // Only User2 should remain
}

TEST_F(CompiledDeleteTest, DeleteWithInCondition) {
    EXPECT_TRUE(executor_->execute("INSERT INTO users (name, age) VALUES ('User1', 18)"));
    EXPECT_TRUE(executor_->execute("INSERT INTO users (name, age) VALUES ('User2', 19)"));
    EXPECT_TRUE(executor_->execute("INSERT INTO users (name, age) VALUES ('User3', 20)"));
    EXPECT_TRUE(executor_->execute("INSERT INTO users (name, age) VALUES ('User4', 25)"));

    auto query = library_->produce<del::DeleteWithIn>();
    auto result = executor_->execute(query);

    ASSERT_TRUE(result.is_success()) << "Delete failed: " << result.error<ErrorContext>();

    EXPECT_EQ(CountRows(), 1);  // Only User4 should remain

    auto select_result = executor_->execute("SELECT age FROM users");
    ASSERT_TRUE(select_result.is_success());
    EXPECT_EQ(select_result.value().get<int>(0, 0), 25);
}

TEST_F(CompiledDeleteTest, DeleteWithBetweenCondition) {
    EXPECT_TRUE(executor_->execute("INSERT INTO users (name, age) VALUES ('User1', 15)"));
    EXPECT_TRUE(executor_->execute("INSERT INTO users (name, age) VALUES ('User2', 20)"));
    EXPECT_TRUE(executor_->execute("INSERT INTO users (name, age) VALUES ('User3', 25)"));
    EXPECT_TRUE(executor_->execute("INSERT INTO users (name, age) VALUES ('User4', 30)"));

    auto query = library_->produce<del::DeleteWithBetween>();
    auto result = executor_->execute(query);

    ASSERT_TRUE(result.is_success()) << "Delete failed: " << result.error<ErrorContext>();

    EXPECT_EQ(CountRows(), 2);  // User1 and User4 should remain
}

// ============== DELETE All Rows ==============

TEST_F(CompiledDeleteTest, DeleteAllRows) {
    EXPECT_TRUE(executor_->execute("INSERT INTO users (name, age) VALUES ('User1', 25)"));
    EXPECT_TRUE(executor_->execute("INSERT INTO users (name, age) VALUES ('User2', 30)"));
    EXPECT_TRUE(executor_->execute("INSERT INTO users (name, age) VALUES ('User3', 35)"));

    auto query = library_->produce<del::DeleteWithoutWhere>();
    auto result = executor_->execute(query);

    ASSERT_TRUE(result.is_success()) << "Delete failed: " << result.error<ErrorContext>();

    EXPECT_EQ(CountRows(), 0);
}

// ============== DELETE with Table Name String ==============

TEST_F(CompiledDeleteTest, DeleteWithTableName) {
    EXPECT_TRUE(executor_->execute("INSERT INTO users (name, age) VALUES ('TestUser', 25)"));

    const auto& s = library_->schemas().users();
    auto query          = delete_from("users").where(s.name == std::string{"TestUser"});
    auto compiled_query = library_->compiler().compile(query);

    auto result = executor_->execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Delete failed: " << result.error<ErrorContext>();

    EXPECT_EQ(CountRows(), 0);
}

// ============== DELETE Edge Cases ==============

TEST_F(CompiledDeleteTest, DeleteNoMatch) {
    EXPECT_TRUE(executor_->execute("INSERT INTO users (name, age) VALUES ('TestUser', 25)"));

    const auto& s = library_->schemas().users();
    auto query          = delete_from(s.table).where(s.age > 100);
    auto compiled_query = library_->compiler().compile(query);

    auto result = executor_->execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Delete failed: " << result.error<ErrorContext>();

    EXPECT_EQ(CountRows(), 1);  // No rows deleted
}

TEST_F(CompiledDeleteTest, DeleteEmptyTable) {
    const auto& s = library_->schemas().users();
    auto query          = delete_from(s.table).where(s.active == false);
    auto compiled_query = library_->compiler().compile(query);

    auto result = executor_->execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Delete failed: " << result.error<ErrorContext>();

    EXPECT_EQ(CountRows(), 0);
}

TEST_F(CompiledDeleteTest, DeleteWithNullComparison) {
    EXPECT_TRUE(executor_->execute("INSERT INTO users (name, age) VALUES ('User1', NULL)"));
    EXPECT_TRUE(executor_->execute("INSERT INTO users (name, age) VALUES ('User2', 30)"));

    const auto& s = library_->schemas().users();
    auto query          = delete_from(s.table).where(s.age == 30);
    auto compiled_query = library_->compiler().compile(query);

    auto result = executor_->execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Delete failed: " << result.error<ErrorContext>();

    EXPECT_EQ(CountRows(), 1);  // Only User1 (with NULL age) remains
}

TEST_F(CompiledDeleteTest, DeleteMultipleSeparateQueries) {
    EXPECT_TRUE(executor_->execute("INSERT INTO users (name, age) VALUES ('User1', 20)"));
    EXPECT_TRUE(executor_->execute("INSERT INTO users (name, age) VALUES ('User2', 30)"));
    EXPECT_TRUE(executor_->execute("INSERT INTO users (name, age) VALUES ('User3', 40)"));

    const auto& s = library_->schemas().users();

    // First delete
    auto query1          = delete_from(s.table).where(s.age == 20);
    auto compiled_query1 = library_->compiler().compile(query1);
    auto result1         = executor_->execute(compiled_query1);
    ASSERT_TRUE(result1.is_success()) << "First delete failed: " << result1.error<ErrorContext>();
    EXPECT_EQ(CountRows(), 2);

    // Second delete
    auto query2          = delete_from(s.table).where(s.age == 40);
    auto compiled_query2 = library_->compiler().compile(query2);
    auto result2         = executor_->execute(compiled_query2);
    ASSERT_TRUE(result2.is_success()) << "Second delete failed: " << result2.error<ErrorContext>();
    EXPECT_EQ(CountRows(), 1);

    // Verify only User2 remains
    auto select_result = executor_->execute("SELECT age FROM users");
    ASSERT_TRUE(select_result.is_success());
    EXPECT_EQ(select_result.value().get<int>(0, 0), 30);
}

TEST_F(CompiledDeleteTest, DeleteWithStringComparison) {
    EXPECT_TRUE(executor_->execute("INSERT INTO users (name, age) VALUES ('Alice', 25)"));
    EXPECT_TRUE(executor_->execute("INSERT INTO users (name, age) VALUES ('Bob', 30)"));
    EXPECT_TRUE(executor_->execute("INSERT INTO users (name, age) VALUES ('Charlie', 35)"));

    const auto& s = library_->schemas().users();
    auto query          = delete_from(s.table).where(s.name == std::string{"Bob"});
    auto compiled_query = library_->compiler().compile(query);

    auto result = executor_->execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Delete failed: " << result.error<ErrorContext>();

    EXPECT_EQ(CountRows(), 2);

    auto select_result = executor_->execute("SELECT COUNT(*) FROM users WHERE name = 'Bob'");
    ASSERT_TRUE(select_result.is_success());
    EXPECT_EQ(select_result.value().get<int>(0, 0), 0);
}

TEST_F(CompiledDeleteTest, DeleteWithBooleanCondition) {
    EXPECT_TRUE(executor_->execute("INSERT INTO users (name, active) VALUES ('User1', true)"));
    EXPECT_TRUE(executor_->execute("INSERT INTO users (name, active) VALUES ('User2', false)"));
    EXPECT_TRUE(executor_->execute("INSERT INTO users (name, active) VALUES ('User3', true)"));

    auto query = library_->produce<del::DeleteWhere>();
    auto result = executor_->execute(query);

    ASSERT_TRUE(result.is_success()) << "Delete failed: " << result.error<ErrorContext>();

    EXPECT_EQ(CountRows(), 2);  // Only active users remain

    auto select_result = executor_->execute("SELECT COUNT(*) FROM users WHERE active = true");
    ASSERT_TRUE(select_result.is_success());
    EXPECT_EQ(select_result.value().get<int>(0, 0), 2);
}
