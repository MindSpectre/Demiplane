// Compiled CONDITION Query Functional Tests
// Tests query compilation + execution with SyncExecutor using QueryLibrary

#include <demiplane/nexus>
#include <demiplane/scroll>

#include <gtest/gtest.h>

#include "postgres_dialect.hpp"
#include "postgres_sync_executor.hpp"
#include "query_library.hpp"

using namespace demiplane::db;
using namespace demiplane::db::postgres;
using namespace demiplane::test;

class CompiledConditionTest : public ::testing::Test {
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

        CreateTables();
        InsertTestData();
    }

    void TearDown() override {
        if (conn_) {
            DropTables();
            PQfinish(conn_);
            conn_ = nullptr;
        }
    }

    void CreateTables() {
        // Create users table
        auto result = executor_->execute(R"(
            CREATE TABLE IF NOT EXISTS users (
                id SERIAL PRIMARY KEY,
                name VARCHAR(255),
                age INTEGER,
                active BOOLEAN
            )
        )");
        ASSERT_TRUE(result.is_success()) << "Failed to create users table";

        // Create posts table
        result = executor_->execute(R"(
            CREATE TABLE IF NOT EXISTS posts (
                id SERIAL PRIMARY KEY,
                user_id INTEGER REFERENCES users(id),
                title VARCHAR(255),
                published BOOLEAN
            )
        )");
        ASSERT_TRUE(result.is_success()) << "Failed to create posts table";
    }

    void DropTables() {
        (void)executor_->execute("DROP TABLE IF EXISTS posts CASCADE");
        (void)executor_->execute("DROP TABLE IF EXISTS users CASCADE");
    }

    void InsertTestData() {
        // Insert users with varied data for condition testing
        ASSERT_TRUE(executor_->execute(
            "INSERT INTO users (id, name, age, active) VALUES (1, 'john', 25, true)"));
        ASSERT_TRUE(executor_->execute(
            "INSERT INTO users (id, name, age, active) VALUES (2, 'jane', 30, true)"));
        ASSERT_TRUE(executor_->execute(
            "INSERT INTO users (id, name, age, active) VALUES (3, 'bob', 18, false)"));
        ASSERT_TRUE(executor_->execute(
            "INSERT INTO users (id, name, age, active) VALUES (4, 'alice', 65, true)"));
        ASSERT_TRUE(executor_->execute(
            "INSERT INTO users (id, name, age, active) VALUES (5, 'charlie', 70, false)"));

        // Insert posts
        ASSERT_TRUE(executor_->execute(
            "INSERT INTO posts (id, user_id, title, published) VALUES (1, 1, 'Post 1', true)"));
        ASSERT_TRUE(executor_->execute(
            "INSERT INTO posts (id, user_id, title, published) VALUES (2, 2, 'Post 2', true)"));
    }

    PGconn* conn_{nullptr};
    std::unique_ptr<SyncExecutor> executor_;
    std::unique_ptr<QueryLibrary> library_;
};

// ============== Binary Comparison Tests ==============

TEST_F(CompiledConditionTest, BinaryEqual) {
    auto query  = library_->produce<condition::BinaryEqual>();
    auto result = executor_->execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_EQ(block.rows(), 1);  // Only john has age == 25
}

TEST_F(CompiledConditionTest, BinaryNotEqual) {
    auto query  = library_->produce<condition::BinaryNotEqual>();
    auto result = executor_->execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_EQ(block.rows(), 4);  // Everyone except age == 25
}

TEST_F(CompiledConditionTest, BinaryGreater) {
    auto query  = library_->produce<condition::BinaryGreater>();
    auto result = executor_->execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_GE(block.rows(), 1);  // Users with age > 18
}

TEST_F(CompiledConditionTest, BinaryGreaterEqual) {
    auto query  = library_->produce<condition::BinaryGreaterEqual>();
    auto result = executor_->execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_GE(block.rows(), 1);  // Users with age >= 18
}

TEST_F(CompiledConditionTest, BinaryLess) {
    auto query  = library_->produce<condition::BinaryLess>();
    auto result = executor_->execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_GE(block.rows(), 1);  // Users with age < 65
}

TEST_F(CompiledConditionTest, BinaryLessEqual) {
    auto query  = library_->produce<condition::BinaryLessEqual>();
    auto result = executor_->execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_GE(block.rows(), 1);  // Users with age <= 65
}

// ============== Logical Operator Tests ==============

TEST_F(CompiledConditionTest, LogicalAnd) {
    auto query  = library_->produce<condition::LogicalAnd>();
    auto result = executor_->execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    // Users with age > 18 AND active == true
    EXPECT_GE(block.rows(), 1);
}

TEST_F(CompiledConditionTest, LogicalOr) {
    auto query  = library_->produce<condition::LogicalOr>();
    auto result = executor_->execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    // Users with age < 18 OR age > 65
    EXPECT_GE(block.rows(), 1);
}

TEST_F(CompiledConditionTest, UnaryCondition) {
    auto query  = library_->produce<condition::UnaryCondition>();
    auto result = executor_->execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    // Users with active == false
    EXPECT_EQ(block.rows(), 2);  // bob and charlie
}

// ============== String Comparison Tests ==============

TEST_F(CompiledConditionTest, StringComparison) {
    auto query  = library_->produce<condition::StringComparison>();
    auto result = executor_->execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_EQ(block.rows(), 1);  // Only john
}

// ============== Range Tests ==============

TEST_F(CompiledConditionTest, Between) {
    auto query  = library_->produce<condition::Between>();
    auto result = executor_->execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    // Users with age BETWEEN 18 AND 65
    EXPECT_GE(block.rows(), 1);
}

TEST_F(CompiledConditionTest, InList) {
    auto query  = library_->produce<condition::InList>();
    auto result = executor_->execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    // Users with age IN (18, 25, 30)
    EXPECT_GE(block.rows(), 1);
}

// ============== Exists Tests ==============

TEST_F(CompiledConditionTest, ExistsCondition) {
    auto query  = library_->produce<condition::ExistsCondition>();
    auto result = executor_->execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
}

TEST_F(CompiledConditionTest, SubqueryCondition) {
    auto query  = library_->produce<condition::SubqueryCondition>();
    auto result = executor_->execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
}

// ============== Complex Nested Tests ==============

TEST_F(CompiledConditionTest, ComplexNested) {
    auto query  = library_->produce<condition::ComplexNested>();
    auto result = executor_->execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    // Complex nested: (age > 18 && age < 65) || (active == true && age >= 65)
}
