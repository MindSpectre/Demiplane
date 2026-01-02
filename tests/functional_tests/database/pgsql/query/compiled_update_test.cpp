// Compiled UPDATE Query Functional Tests
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

// Test fixture for compiled UPDATE queries
class CompiledUpdateTest : public ::testing::Test {
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

        std::string conn_info = "host=" + std::string(host) + " port=" + std::string(port) +
                                " dbname=" + std::string(dbname) + " user=" + std::string(user) +
                                " password=" + std::string(password);

        conn_ = PQconnectdb(conn_info.c_str());

        if (PQstatus(conn_) != CONNECTION_OK) {
            const std::string error = PQerrorMessage(conn_);
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

    PGconn* conn_{nullptr};
    std::unique_ptr<SyncExecutor> executor_;
    std::unique_ptr<QueryLibrary> library_;
};

// ============== Basic UPDATE Tests ==============

TEST_F(CompiledUpdateTest, UpdateSingleColumn) {
    EXPECT_TRUE(executor_->execute("INSERT INTO users (name, age, active) VALUES ('Alice', 30, true)"));

    const auto& s = library_->schemas().users();
    auto query          = update(s.table).set("age", 31).where(s.name == std::string{"Alice"});
    auto compiled_query = library_->compiler().compile(query);

    auto result = executor_->execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Update failed: " << result.error<ErrorContext>();

    auto select_result = executor_->execute("SELECT age FROM users WHERE name = 'Alice'");
    ASSERT_TRUE(select_result.is_success());
    auto& block = select_result.value();
    EXPECT_EQ(block.rows(), 1);
    EXPECT_EQ(block.get<int>(0, 0), 31);
}

TEST_F(CompiledUpdateTest, UpdateMultipleColumns) {
    EXPECT_TRUE(executor_->execute("INSERT INTO users (name, age, active) VALUES ('Bob', 25, false)"));

    const auto& s = library_->schemas().users();
    auto query = update(s.table).set("age", 26).set("active", true).where(s.name == std::string{"Bob"});
    auto compiled_query = library_->compiler().compile(query);

    auto result = executor_->execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Update failed: " << result.error<ErrorContext>();

    auto select_result = executor_->execute("SELECT age, active FROM users WHERE name = 'Bob'");
    ASSERT_TRUE(select_result.is_success());
    auto& block = select_result.value();
    EXPECT_EQ(block.rows(), 1);
    EXPECT_EQ(block.get<int>(0, 0), 26);
    EXPECT_EQ(block.get<bool>(0, 1), true);
}

TEST_F(CompiledUpdateTest, UpdateWithInitializerList) {
    EXPECT_TRUE(executor_->execute("INSERT INTO users (name, age, active) VALUES ('Charlie', 35, true)"));

    const auto& s = library_->schemas().users();
    auto query = update(s.table)
                     .set({
                         {"age",    36   },
                         {"active", false}
    })
                     .where(s.name == std::string{"Charlie"});
    auto compiled_query = library_->compiler().compile(query);

    auto result = executor_->execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Update failed: " << result.error<ErrorContext>();

    auto select_result = executor_->execute("SELECT age, active FROM users WHERE name = 'Charlie'");
    ASSERT_TRUE(select_result.is_success());
    auto& block = select_result.value();
    EXPECT_EQ(block.rows(), 1);
    EXPECT_EQ(block.get<int>(0, 0), 36);
    EXPECT_EQ(block.get<bool>(0, 1), false);
}

// ============== UPDATE with WHERE Conditions ==============

TEST_F(CompiledUpdateTest, UpdateWithSimpleWhere) {
    EXPECT_TRUE(executor_->execute("INSERT INTO users (name, age, active) VALUES ('User1', 20, true)"));
    EXPECT_TRUE(executor_->execute("INSERT INTO users (name, age, active) VALUES ('User2', 30, true)"));
    EXPECT_TRUE(executor_->execute("INSERT INTO users (name, age, active) VALUES ('User3', 40, true)"));

    const auto& s = library_->schemas().users();
    auto query          = update(s.table).set("active", false).where(s.age > 25);
    auto compiled_query = library_->compiler().compile(query);

    auto result = executor_->execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Update failed: " << result.error<ErrorContext>();

    auto select_result = executor_->execute("SELECT COUNT(*) FROM users WHERE active = false");
    ASSERT_TRUE(select_result.is_success());
    EXPECT_EQ(select_result.value().get<int>(0, 0), 2);
}

TEST_F(CompiledUpdateTest, UpdateWithComplexWhere) {
    EXPECT_TRUE(executor_->execute("INSERT INTO users (name, age, active) VALUES ('User1', 25, true)"));
    EXPECT_TRUE(executor_->execute("INSERT INTO users (name, age, active) VALUES ('User2', 30, true)"));
    EXPECT_TRUE(executor_->execute("INSERT INTO users (name, age, active) VALUES ('User3', 35, false)"));

    const auto& s = library_->schemas().users();
    auto query          = update(s.table).set("age", 40).where((s.age >= 25) && (s.active == true));
    auto compiled_query = library_->compiler().compile(query);

    auto result = executor_->execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Update failed: " << result.error<ErrorContext>();

    auto select_result = executor_->execute("SELECT COUNT(*) FROM users WHERE age = 40");
    ASSERT_TRUE(select_result.is_success());
    EXPECT_EQ(select_result.value().get<int>(0, 0), 2);
}

TEST_F(CompiledUpdateTest, UpdateWithOrCondition) {
    EXPECT_TRUE(executor_->execute("INSERT INTO users (name, age, active) VALUES ('User1', 20, true)"));
    EXPECT_TRUE(executor_->execute("INSERT INTO users (name, age, active) VALUES ('User2', 30, false)"));
    EXPECT_TRUE(executor_->execute("INSERT INTO users (name, age, active) VALUES ('User3', 40, true)"));

    const auto& s = library_->schemas().users();
    auto query          = update(s.table).set("age", 50).where((s.age < 25) || (s.age > 35));
    auto compiled_query = library_->compiler().compile(query);

    auto result = executor_->execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Update failed: " << result.error<ErrorContext>();

    auto select_result = executor_->execute("SELECT COUNT(*) FROM users WHERE age = 50");
    ASSERT_TRUE(select_result.is_success());
    EXPECT_EQ(select_result.value().get<int>(0, 0), 2);
}

// ============== UPDATE without WHERE (all rows) ==============

TEST_F(CompiledUpdateTest, UpdateAllRows) {
    EXPECT_TRUE(executor_->execute("INSERT INTO users (name, age, active) VALUES ('User1', 25, true)"));
    EXPECT_TRUE(executor_->execute("INSERT INTO users (name, age, active) VALUES ('User2', 30, false)"));
    EXPECT_TRUE(executor_->execute("INSERT INTO users (name, age, active) VALUES ('User3', 35, true)"));

    auto query = library_->produce<upd::UpdateWithoutWhere>();
    auto result = executor_->execute(query);

    ASSERT_TRUE(result.is_success()) << "Update failed: " << result.error<ErrorContext>();

    auto select_result = executor_->execute("SELECT COUNT(*) FROM users WHERE active = true");
    ASSERT_TRUE(select_result.is_success());
    EXPECT_EQ(select_result.value().get<int>(0, 0), 3);
}

// ============== UPDATE with Different Data Types ==============

TEST_F(CompiledUpdateTest, UpdateString) {
    EXPECT_TRUE(executor_->execute("INSERT INTO users (name, age) VALUES ('OldName', 30)"));

    const auto& s = library_->schemas().users();
    auto query          = update(s.table).set("name", std::string{"NewName"}).where(s.age == 30);
    auto compiled_query = library_->compiler().compile(query);

    auto result = executor_->execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Update failed: " << result.error<ErrorContext>();

    auto select_result = executor_->execute("SELECT name FROM users WHERE age = 30");
    ASSERT_TRUE(select_result.is_success());
    auto& block = select_result.value();
    EXPECT_EQ(block.rows(), 1);
    EXPECT_EQ(block.get<std::string>(0, 0), "NewName");
}

TEST_F(CompiledUpdateTest, UpdateBoolean) {
    EXPECT_TRUE(executor_->execute("INSERT INTO users (name, active) VALUES ('TestUser', true)"));

    const auto& s = library_->schemas().users();
    auto query          = update(s.table).set("active", false).where(s.name == std::string{"TestUser"});
    auto compiled_query = library_->compiler().compile(query);

    auto result = executor_->execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Update failed: " << result.error<ErrorContext>();

    auto select_result = executor_->execute("SELECT active FROM users WHERE name = 'TestUser'");
    ASSERT_TRUE(select_result.is_success());
    auto& block = select_result.value();
    EXPECT_EQ(block.rows(), 1);
    EXPECT_EQ(block.get<bool>(0, 0), false);
}

TEST_F(CompiledUpdateTest, UpdateInteger) {
    EXPECT_TRUE(executor_->execute("INSERT INTO users (name, age) VALUES ('TestUser', 25)"));

    const auto& s = library_->schemas().users();
    auto query          = update(s.table).set("age", 50).where(s.name == std::string{"TestUser"});
    auto compiled_query = library_->compiler().compile(query);

    auto result = executor_->execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Update failed: " << result.error<ErrorContext>();

    auto select_result = executor_->execute("SELECT age FROM users WHERE name = 'TestUser'");
    ASSERT_TRUE(select_result.is_success());
    auto& block = select_result.value();
    EXPECT_EQ(block.rows(), 1);
    EXPECT_EQ(block.get<int>(0, 0), 50);
}

// ============== UPDATE with NULL Values ==============

TEST_F(CompiledUpdateTest, UpdateToNull) {
    EXPECT_TRUE(executor_->execute("INSERT INTO users (name, age) VALUES ('TestUser', 30)"));

    const auto& s = library_->schemas().users();
    auto query = update(s.table).set("age", std::monostate{}).where(s.name == std::string{"TestUser"});
    auto compiled_query = library_->compiler().compile(query);

    auto result = executor_->execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Update to NULL failed: " << result.error<ErrorContext>();

    auto select_result = executor_->execute("SELECT age FROM users WHERE name = 'TestUser'");
    ASSERT_TRUE(select_result.is_success());
    auto& block = select_result.value();
    EXPECT_EQ(block.rows(), 1);
    auto age_opt = block.get_opt<int>(0, 0);
    EXPECT_FALSE(age_opt.has_value()) << "Age should be NULL";
}

// ============== UPDATE with Table Name String ==============

TEST_F(CompiledUpdateTest, UpdateWithTableName) {
    EXPECT_TRUE(executor_->execute("INSERT INTO users (name, age) VALUES ('TestUser', 25)"));

    const auto& s = library_->schemas().users();
    auto query          = update("users").set("age", 35).where(s.name == std::string{"TestUser"});
    auto compiled_query = library_->compiler().compile(query);

    auto result = executor_->execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Update failed: " << result.error<ErrorContext>();

    auto select_result = executor_->execute("SELECT age FROM users WHERE name = 'TestUser'");
    ASSERT_TRUE(select_result.is_success());
    EXPECT_EQ(select_result.value().get<int>(0, 0), 35);
}

// ============== UPDATE Edge Cases ==============

TEST_F(CompiledUpdateTest, UpdateNoMatch) {
    EXPECT_TRUE(executor_->execute("INSERT INTO users (name, age) VALUES ('TestUser', 25)"));

    const auto& s = library_->schemas().users();
    auto query          = update(s.table).set("age", 50).where(s.age > 100);
    auto compiled_query = library_->compiler().compile(query);

    auto result = executor_->execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Update failed: " << result.error<ErrorContext>();

    auto select_result = executor_->execute("SELECT age FROM users WHERE name = 'TestUser'");
    ASSERT_TRUE(select_result.is_success());
    EXPECT_EQ(select_result.value().get<int>(0, 0), 25);  // Original value
}

TEST_F(CompiledUpdateTest, UpdateEmptyTable) {
    const auto& s = library_->schemas().users();
    auto query          = update(s.table).set("active", false);
    auto compiled_query = library_->compiler().compile(query);

    auto result = executor_->execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Update failed: " << result.error<ErrorContext>();

    auto select_result = executor_->execute("SELECT COUNT(*) FROM users");
    ASSERT_TRUE(select_result.is_success());
    EXPECT_EQ(select_result.value().get<int>(0, 0), 0);
}

TEST_F(CompiledUpdateTest, UpdateToSameValue) {
    EXPECT_TRUE(executor_->execute("INSERT INTO users (name, age) VALUES ('TestUser', 30)"));

    const auto& s = library_->schemas().users();
    auto query          = update(s.table).set("age", 30).where(s.name == std::string{"TestUser"});
    auto compiled_query = library_->compiler().compile(query);

    auto result = executor_->execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Update failed: " << result.error<ErrorContext>();

    auto select_result = executor_->execute("SELECT age FROM users WHERE name = 'TestUser'");
    ASSERT_TRUE(select_result.is_success());
    EXPECT_EQ(select_result.value().get<int>(0, 0), 30);
}
