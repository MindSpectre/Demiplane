// Compiled AGGREGATE Query Functional Tests
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

class CompiledAggregateTest : public ::testing::Test {
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
        // Create users table with department and salary for aggregate tests
        auto result = executor_->execute(R"(
            CREATE TABLE IF NOT EXISTS users (
                id SERIAL PRIMARY KEY,
                name VARCHAR(255),
                age INTEGER,
                active BOOLEAN,
                department VARCHAR(100),
                salary DECIMAL(10,2)
            )
        )");
        ASSERT_TRUE(result.is_success()) << "Failed to create users table";

        // Create orders table
        result = executor_->execute(R"(
            CREATE TABLE IF NOT EXISTS orders (
                id SERIAL PRIMARY KEY,
                user_id INTEGER,
                amount DECIMAL(10,2),
                completed BOOLEAN
            )
        )");
        ASSERT_TRUE(result.is_success()) << "Failed to create orders table";
    }

    void DropTables() {
        (void)executor_->execute("DROP TABLE IF EXISTS orders CASCADE");
        (void)executor_->execute("DROP TABLE IF EXISTS users CASCADE");
    }

    void InsertTestData() {
        // Insert users with department and salary
        ASSERT_TRUE(executor_->execute(
            "INSERT INTO users (id, name, age, active, department, salary) VALUES "
            "(1, 'Alice', 30, true, 'Engineering', 75000.00)"));
        ASSERT_TRUE(executor_->execute(
            "INSERT INTO users (id, name, age, active, department, salary) VALUES "
            "(2, 'Bob', 25, true, 'Engineering', 65000.00)"));
        ASSERT_TRUE(executor_->execute(
            "INSERT INTO users (id, name, age, active, department, salary) VALUES "
            "(3, 'Charlie', 35, false, 'Sales', 55000.00)"));
        ASSERT_TRUE(executor_->execute(
            "INSERT INTO users (id, name, age, active, department, salary) VALUES "
            "(4, 'Diana', 28, true, 'Sales', 60000.00)"));
        ASSERT_TRUE(executor_->execute(
            "INSERT INTO users (id, name, age, active, department, salary) VALUES "
            "(5, 'Eve', 32, true, 'Marketing', 70000.00)"));

        // Insert orders
        ASSERT_TRUE(executor_->execute(
            "INSERT INTO orders (id, user_id, amount, completed) VALUES (1, 1, 100.00, true)"));
        ASSERT_TRUE(executor_->execute(
            "INSERT INTO orders (id, user_id, amount, completed) VALUES (2, 1, 200.00, false)"));
        ASSERT_TRUE(executor_->execute(
            "INSERT INTO orders (id, user_id, amount, completed) VALUES (3, 2, 150.00, true)"));
        ASSERT_TRUE(executor_->execute(
            "INSERT INTO orders (id, user_id, amount, completed) VALUES (4, 3, 300.00, true)"));
    }

    PGconn* conn_{nullptr};
    std::unique_ptr<SyncExecutor> executor_;
    std::unique_ptr<QueryLibrary> library_;
};

// ============== Basic Aggregate Tests ==============

TEST_F(CompiledAggregateTest, Count) {
    auto query  = library_->produce<aggregate::Count>();
    auto result = executor_->execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_EQ(block.rows(), 1);
    EXPECT_EQ(block.cols(), 1);
}

TEST_F(CompiledAggregateTest, Sum) {
    auto query  = library_->produce<aggregate::Sum>();
    auto result = executor_->execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_EQ(block.rows(), 1);
}

TEST_F(CompiledAggregateTest, Avg) {
    auto query  = library_->produce<aggregate::Avg>();
    auto result = executor_->execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_EQ(block.rows(), 1);
}

TEST_F(CompiledAggregateTest, Min) {
    auto query  = library_->produce<aggregate::Min>();
    auto result = executor_->execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_EQ(block.rows(), 1);
}

TEST_F(CompiledAggregateTest, Max) {
    auto query  = library_->produce<aggregate::Max>();
    auto result = executor_->execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_EQ(block.rows(), 1);
}

// ============== Advanced Aggregate Tests ==============

TEST_F(CompiledAggregateTest, AggregateWithAlias) {
    auto query  = library_->produce<aggregate::AggregateWithAlias>();
    auto result = executor_->execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
}

TEST_F(CompiledAggregateTest, CountDistinct) {
    auto query  = library_->produce<aggregate::CountDistinct>();
    auto result = executor_->execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
}

TEST_F(CompiledAggregateTest, CountAll) {
    auto query  = library_->produce<aggregate::CountAll>();
    auto result = executor_->execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_EQ(block.rows(), 1);
}

TEST_F(CompiledAggregateTest, AggregateGroupBy) {
    auto query  = library_->produce<aggregate::AggregateGroupBy>();
    auto result = executor_->execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    // Should have groups for Engineering, Sales, Marketing
    EXPECT_GE(block.rows(), 1);
}

TEST_F(CompiledAggregateTest, AggregateHaving) {
    auto query  = library_->produce<aggregate::AggregateHaving>();
    auto result = executor_->execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
}

TEST_F(CompiledAggregateTest, MultipleAggregates) {
    auto query  = library_->produce<aggregate::MultipleAggregates>();
    auto result = executor_->execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    // Multiple aggregate columns
    EXPECT_GE(block.cols(), 2);
}

TEST_F(CompiledAggregateTest, AggregateMixedTypes) {
    auto query  = library_->produce<aggregate::AggregateMixedTypes>();
    auto result = executor_->execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
}
