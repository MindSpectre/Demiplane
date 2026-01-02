// Compiled JOIN Query Functional Tests
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

class CompiledJoinTest : public ::testing::Test {
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

        // Create comments table
        result = executor_->execute(R"(
            CREATE TABLE IF NOT EXISTS comments (
                id SERIAL PRIMARY KEY,
                post_id INTEGER REFERENCES posts(id),
                user_id INTEGER REFERENCES users(id),
                content TEXT
            )
        )");
        ASSERT_TRUE(result.is_success()) << "Failed to create comments table";
    }

    void DropTables() {
        (void)executor_->execute("DROP TABLE IF EXISTS comments CASCADE");
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

        // Insert posts
        ASSERT_TRUE(executor_->execute(
            "INSERT INTO posts (id, user_id, title, published) VALUES (1, 1, 'Post by Alice', true)"));
        ASSERT_TRUE(executor_->execute(
            "INSERT INTO posts (id, user_id, title, published) VALUES (2, 1, 'Another Alice Post', false)"));
        ASSERT_TRUE(executor_->execute(
            "INSERT INTO posts (id, user_id, title, published) VALUES (3, 2, 'Bob Post', true)"));

        // Insert orders
        ASSERT_TRUE(executor_->execute(
            "INSERT INTO orders (id, user_id, amount, completed) VALUES (1, 1, 100.00, true)"));
        ASSERT_TRUE(executor_->execute(
            "INSERT INTO orders (id, user_id, amount, completed) VALUES (2, 1, 200.00, false)"));
        ASSERT_TRUE(executor_->execute(
            "INSERT INTO orders (id, user_id, amount, completed) VALUES (3, 2, 150.00, true)"));

        // Insert comments
        ASSERT_TRUE(executor_->execute(
            "INSERT INTO comments (id, post_id, user_id, content) VALUES (1, 1, 2, 'Nice post!')"));
        ASSERT_TRUE(executor_->execute(
            "INSERT INTO comments (id, post_id, user_id, content) VALUES (2, 1, 3, 'Great work!')"));
    }

    PGconn* conn_{nullptr};
    std::unique_ptr<SyncExecutor> executor_;
    std::unique_ptr<QueryLibrary> library_;
};

// ============== INNER JOIN Tests ==============

TEST_F(CompiledJoinTest, InnerJoin) {
    auto query  = library_->produce<join::InnerJoin>();
    auto result = executor_->execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_GE(block.rows(), 1);
}

TEST_F(CompiledJoinTest, LeftJoin) {
    auto query  = library_->produce<join::LeftJoin>();
    auto result = executor_->execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    // Left join should include all users, even those without posts
    EXPECT_GE(block.rows(), 3);
}

TEST_F(CompiledJoinTest, RightJoin) {
    auto query  = library_->produce<join::RightJoin>();
    auto result = executor_->execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    // Right join should include all posts
    EXPECT_GE(block.rows(), 1);
}

TEST_F(CompiledJoinTest, MultipleJoins) {
    auto query  = library_->produce<join::MultipleJoins>();
    auto result = executor_->execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
}

TEST_F(CompiledJoinTest, JoinComplexCondition) {
    auto query  = library_->produce<join::JoinComplexCondition>();
    auto result = executor_->execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
}

TEST_F(CompiledJoinTest, JoinWithWhere) {
    auto query  = library_->produce<join::JoinWithWhere>();
    auto result = executor_->execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
}

TEST_F(CompiledJoinTest, JoinWithAggregates) {
    auto query  = library_->produce<join::JoinWithAggregates>();
    auto result = executor_->execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
}

TEST_F(CompiledJoinTest, JoinWithOrderBy) {
    auto query  = library_->produce<join::JoinWithOrderBy>();
    auto result = executor_->execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
}
