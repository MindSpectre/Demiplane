// Compiled SUBQUERY Query Functional Tests
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

class CompiledSubqueryTest : public ::testing::Test {
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

        // Create orders table
        result = executor_->execute(R"(
            CREATE TABLE IF NOT EXISTS orders (
                id SERIAL PRIMARY KEY,
                user_id INTEGER REFERENCES users(id),
                amount DECIMAL(10,2),
                completed BOOLEAN
            )
        )");
        ASSERT_TRUE(result.is_success()) << "Failed to create orders table";
    }

    void DropTables() {
        (void)executor_->execute("DROP TABLE IF EXISTS orders CASCADE");
        (void)executor_->execute("DROP TABLE IF EXISTS posts CASCADE");
        (void)executor_->execute("DROP TABLE IF EXISTS users CASCADE");
    }

    void InsertTestData() {
        // Insert users
        ASSERT_TRUE(executor_->execute(
            "INSERT INTO users (id, name, age, active) VALUES (1, 'Alice', 30, true)"));
        ASSERT_TRUE(executor_->execute(
            "INSERT INTO users (id, name, age, active) VALUES (2, 'Bob', 25, true)"));
        ASSERT_TRUE(executor_->execute(
            "INSERT INTO users (id, name, age, active) VALUES (3, 'Charlie', 35, false)"));
        ASSERT_TRUE(executor_->execute(
            "INSERT INTO users (id, name, age, active) VALUES (4, 'Diana', 28, true)"));

        // Insert posts
        ASSERT_TRUE(executor_->execute(
            "INSERT INTO posts (id, user_id, title, published) VALUES (1, 1, 'Alice Post 1', true)"));
        ASSERT_TRUE(executor_->execute(
            "INSERT INTO posts (id, user_id, title, published) VALUES (2, 1, 'Alice Post 2', true)"));
        ASSERT_TRUE(executor_->execute(
            "INSERT INTO posts (id, user_id, title, published) VALUES (3, 2, 'Bob Post', true)"));
        ASSERT_TRUE(executor_->execute(
            "INSERT INTO posts (id, user_id, title, published) VALUES (4, 3, 'Charlie Draft', false)"));

        // Insert orders
        ASSERT_TRUE(executor_->execute(
            "INSERT INTO orders (id, user_id, amount, completed) VALUES (1, 1, 100.00, true)"));
        ASSERT_TRUE(executor_->execute(
            "INSERT INTO orders (id, user_id, amount, completed) VALUES (2, 1, 500.00, true)"));
        ASSERT_TRUE(executor_->execute(
            "INSERT INTO orders (id, user_id, amount, completed) VALUES (3, 2, 250.00, true)"));
        ASSERT_TRUE(executor_->execute(
            "INSERT INTO orders (id, user_id, amount, completed) VALUES (4, 3, 75.00, false)"));
    }

    PGconn* conn_{nullptr};
    std::unique_ptr<SyncExecutor> executor_;
    std::unique_ptr<QueryLibrary> library_;
};

// ============== Subquery in WHERE Tests ==============

TEST_F(CompiledSubqueryTest, SubqueryInWhere) {
    auto query  = library_->produce<subq::SubqueryInWhere>();
    auto result = executor_->execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    // Should return posts by active users
    EXPECT_GE(block.rows(), 1);
}

// ============== EXISTS Tests ==============

TEST_F(CompiledSubqueryTest, Exists) {
    auto query  = library_->produce<subq::Exists>();
    auto result = executor_->execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    // Should return users who have published posts
    EXPECT_GE(block.rows(), 1);
}

TEST_F(CompiledSubqueryTest, NotExists) {
    auto query  = library_->produce<subq::NotExists>();
    auto result = executor_->execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
}

// ============== IN Subquery Tests ==============

TEST_F(CompiledSubqueryTest, InSubqueryMultiple) {
    auto query  = library_->produce<subq::InSubqueryMultiple>();
    auto result = executor_->execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
}

// ============== Nested Subquery Tests ==============

TEST_F(CompiledSubqueryTest, NestedSubqueries) {
    auto query  = library_->produce<subq::NestedSubqueries>();
    auto result = executor_->execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
}

// ============== Subquery with Aggregates ==============

TEST_F(CompiledSubqueryTest, SubqueryWithAggregates) {
    auto query  = library_->produce<subq::SubqueryWithAggregates>();
    auto result = executor_->execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
}

// ============== Subquery with DISTINCT ==============

TEST_F(CompiledSubqueryTest, SubqueryWithDistinct) {
    auto query  = library_->produce<subq::SubqueryWithDistinct>();
    auto result = executor_->execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
}
