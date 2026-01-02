// Compiled CLAUSE Query Functional Tests
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

class CompiledClauseTest : public ::testing::Test {
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
        // Create users table with extended fields
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

        // Create orders table with extended fields
        result = executor_->execute(R"(
            CREATE TABLE IF NOT EXISTS orders (
                id SERIAL PRIMARY KEY,
                user_id INTEGER REFERENCES users(id),
                amount DECIMAL(10,2),
                completed BOOLEAN,
                status VARCHAR(50),
                created_date DATE
            )
        )");
        ASSERT_TRUE(result.is_success()) << "Failed to create orders table";

        // Create test_table for simple table name tests
        result = executor_->execute(R"(
            CREATE TABLE IF NOT EXISTS test_table (
                id SERIAL PRIMARY KEY,
                value INTEGER
            )
        )");
        ASSERT_TRUE(result.is_success()) << "Failed to create test_table";
    }

    void DropTables() {
        (void)executor_->execute("DROP TABLE IF EXISTS orders CASCADE");
        (void)executor_->execute("DROP TABLE IF EXISTS users CASCADE");
        (void)executor_->execute("DROP TABLE IF EXISTS test_table CASCADE");
    }

    void InsertTestData() {
        // Insert users with department and salary for clause testing
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
        ASSERT_TRUE(executor_->execute(
            "INSERT INTO users (id, name, age, active, department, salary) VALUES "
            "(6, 'Frank', 45, true, 'Engineering', 85000.00)"));
        ASSERT_TRUE(executor_->execute(
            "INSERT INTO users (id, name, age, active, department, salary) VALUES "
            "(7, 'Grace', 22, true, 'Marketing', 50000.00)"));

        // Insert orders
        ASSERT_TRUE(executor_->execute(
            "INSERT INTO orders (id, user_id, amount, completed, status) VALUES "
            "(1, 1, 500.00, true, 'completed')"));
        ASSERT_TRUE(executor_->execute(
            "INSERT INTO orders (id, user_id, amount, completed, status) VALUES "
            "(2, 1, 300.00, true, 'completed')"));
        ASSERT_TRUE(executor_->execute(
            "INSERT INTO orders (id, user_id, amount, completed, status) VALUES "
            "(3, 2, 200.00, false, 'pending')"));
        ASSERT_TRUE(executor_->execute(
            "INSERT INTO orders (id, user_id, amount, completed, status) VALUES "
            "(4, 3, 150.00, true, 'completed')"));
        ASSERT_TRUE(executor_->execute(
            "INSERT INTO orders (id, user_id, amount, completed, status) VALUES "
            "(5, 4, 600.00, true, 'completed')"));

        // Insert into test_table
        ASSERT_TRUE(executor_->execute(
            "INSERT INTO test_table (id, value) VALUES (1, 100)"));
    }

    PGconn* conn_{nullptr};
    std::unique_ptr<SyncExecutor> executor_;
    std::unique_ptr<QueryLibrary> library_;
};

// ============== FROM Clause Tests ==============

TEST_F(CompiledClauseTest, FromTable) {
    auto query  = library_->produce<clause::FromTable>();
    auto result = executor_->execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_GE(block.rows(), 1);
}

TEST_F(CompiledClauseTest, FromTableName) {
    auto query  = library_->produce<clause::FromTableName>();
    auto result = executor_->execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
}

// ============== WHERE Clause Tests ==============

TEST_F(CompiledClauseTest, WhereSimple) {
    auto query  = library_->produce<clause::WhereSimple>();
    auto result = executor_->execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    // Should return only active users
    EXPECT_GE(block.rows(), 1);
}

TEST_F(CompiledClauseTest, WhereComplex) {
    auto query  = library_->produce<clause::WhereComplex>();
    auto result = executor_->execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
}

TEST_F(CompiledClauseTest, WhereIn) {
    auto query  = library_->produce<clause::WhereIn>();
    auto result = executor_->execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
}

TEST_F(CompiledClauseTest, WhereBetween) {
    auto query  = library_->produce<clause::WhereBetween>();
    auto result = executor_->execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
}

// ============== GROUP BY Clause Tests ==============

TEST_F(CompiledClauseTest, GroupBySingle) {
    auto query  = library_->produce<clause::GroupBySingle>();
    auto result = executor_->execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    // Should have groups for each department
    EXPECT_GE(block.rows(), 1);
}

TEST_F(CompiledClauseTest, GroupByMultiple) {
    auto query  = library_->produce<clause::GroupByMultiple>();
    auto result = executor_->execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
}

TEST_F(CompiledClauseTest, GroupByWithWhere) {
    auto query  = library_->produce<clause::GroupByWithWhere>();
    auto result = executor_->execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
}

// ============== HAVING Clause Tests ==============

TEST_F(CompiledClauseTest, HavingSimple) {
    auto query  = library_->produce<clause::HavingSimple>();
    auto result = executor_->execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
}

TEST_F(CompiledClauseTest, HavingMultiple) {
    auto query  = library_->produce<clause::HavingMultiple>();
    auto result = executor_->execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
}

TEST_F(CompiledClauseTest, HavingWithWhere) {
    auto query  = library_->produce<clause::HavingWithWhere>();
    auto result = executor_->execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
}

// ============== ORDER BY Clause Tests ==============

TEST_F(CompiledClauseTest, OrderByAsc) {
    auto query  = library_->produce<clause::OrderByAsc>();
    auto result = executor_->execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_GE(block.rows(), 1);
}

TEST_F(CompiledClauseTest, OrderByDesc) {
    auto query  = library_->produce<clause::OrderByDesc>();
    auto result = executor_->execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_GE(block.rows(), 1);
}

TEST_F(CompiledClauseTest, OrderByMultiple) {
    auto query  = library_->produce<clause::OrderByMultiple>();
    auto result = executor_->execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
}

// ============== LIMIT Clause Tests ==============

TEST_F(CompiledClauseTest, LimitBasic) {
    auto query  = library_->produce<clause::LimitBasic>();
    auto result = executor_->execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_LE(block.rows(), 10);
}

TEST_F(CompiledClauseTest, LimitWithOrderBy) {
    auto query  = library_->produce<clause::LimitWithOrderBy>();
    auto result = executor_->execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_LE(block.rows(), 5);
}

TEST_F(CompiledClauseTest, LimitWithWhereOrderBy) {
    auto query  = library_->produce<clause::LimitWithWhereOrderBy>();
    auto result = executor_->execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_LE(block.rows(), 20);
}

// ============== Complex Combined Clause Tests ==============

TEST_F(CompiledClauseTest, ComplexAllClauses) {
    auto query  = library_->produce<clause::ComplexAllClauses>();
    auto result = executor_->execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
}

TEST_F(CompiledClauseTest, ClausesWithJoins) {
    auto query  = library_->produce<clause::ClausesWithJoins>();
    auto result = executor_->execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
}
