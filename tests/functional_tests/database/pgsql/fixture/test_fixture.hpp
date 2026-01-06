#pragma once

// PostgreSQL Functional Test Fixture
// Shared setup for all PostgreSQL query functional tests

#include <cstdlib>
#include <demiplane/nexus>
#include <demiplane/scroll>
#include <memory>
#include <string>

#include <gtest/gtest.h>
#include <libpq-fe.h>

#include "postgres_dialect.hpp"
#include "postgres_sync_executor.hpp"
#include "query_library.hpp"
#include "test_schemas.hpp"

namespace demiplane::test {

    // Base fixture for PostgreSQL functional tests
    // Provides common setup: logging, connection, executor, query library
    class PgsqlTestFixture : public ::testing::Test {
    protected:
        void SetUp() override {
            InitializeLogging();
            ConnectToDatabase();

            if (conn_ == nullptr) {
                return;  // Test will be skipped
            }

            executor_ = std::make_unique<db::postgres::SyncExecutor>(conn_);
            library_  = std::make_unique<QueryLibrary>(std::make_unique<db::postgres::Dialect>());
        }

        void TearDown() override {
            if (conn_) {
                PQfinish(conn_);
                conn_ = nullptr;
            }
        }

        // Logging initialization (Nexus singleton)
        static void InitializeLogging() {
            static bool initialized = false;
            if (initialized) {
                return;
            }

            nexus::instance().register_singleton<scroll::ConsoleSink<scroll::DetailedEntry>>([] {
                return std::make_shared<scroll::ConsoleSink<scroll::DetailedEntry>>(
                    scroll::ConsoleSinkConfig{}.flush_each_entry(true).threshold(scroll::TRC).finalize());
            });

            nexus::instance().register_singleton<scroll::Logger>([] {
                auto logger = std::make_shared<scroll::Logger>();
                logger->add_sink(nexus::instance().get<scroll::ConsoleSink<scroll::DetailedEntry>>());
                return logger;
            });

            initialized = true;
        }

        // Connect to PostgreSQL using environment variables
        void ConnectToDatabase() {
            const char* host   = std::getenv("POSTGRES_HOST") ? std::getenv("POSTGRES_HOST") : "localhost";
            const char* port   = std::getenv("POSTGRES_PORT") ? std::getenv("POSTGRES_PORT") : "5433";
            const char* dbname = std::getenv("POSTGRES_DB") ? std::getenv("POSTGRES_DB") : "test_db";
            const char* user   = std::getenv("POSTGRES_USER") ? std::getenv("POSTGRES_USER") : "test_user";
            const char* password =
                std::getenv("POSTGRES_PASSWORD") ? std::getenv("POSTGRES_PASSWORD") : "test_password";

            const std::string conninfo = "host=" + std::string(host) + " port=" + std::string(port) +
                                         " dbname=" + std::string(dbname) + " user=" + std::string(user) +
                                         " password=" + std::string(password);

            conn_ = PQconnectdb(conninfo.c_str());

            if (PQstatus(conn_) != CONNECTION_OK) {
                const std::string error = PQerrorMessage(conn_);
                PQfinish(conn_);
                conn_ = nullptr;
                GTEST_SKIP() << "Failed to connect to PostgreSQL: " << error;
            }
        }

        // Table creation helpers using SchemaDDL
        void CreateUsersTable() const {
            const auto result =
                executor_->execute(std::string(SchemaDDL::users_table(db::SupportedProviders::PostgreSQL)));
            ASSERT_TRUE(result.is_success()) << "Failed to create users table";
        }

        void CreateUsersExtendedTable() const {
            const auto result =
                executor_->execute(std::string(SchemaDDL::users_extended_table(db::SupportedProviders::PostgreSQL)));
            ASSERT_TRUE(result.is_success()) << "Failed to create users_extended table";
        }

        void CreatePostsTable() const {
            const auto result =
                executor_->execute(std::string(SchemaDDL::posts_table(db::SupportedProviders::PostgreSQL)));
            ASSERT_TRUE(result.is_success()) << "Failed to create posts table";
        }

        void CreateOrdersTable() const {
            const auto result =
                executor_->execute(std::string(SchemaDDL::orders_table(db::SupportedProviders::PostgreSQL)));
            ASSERT_TRUE(result.is_success()) << "Failed to create orders table";
        }

        void CreateOrdersExtendedTable() const {
            const auto result =
                executor_->execute(std::string(SchemaDDL::orders_extended_table(db::SupportedProviders::PostgreSQL)));
            ASSERT_TRUE(result.is_success()) << "Failed to create orders_extended table";
        }

        void CreateCommentsTable() const {
            const auto result =
                executor_->execute(std::string(SchemaDDL::comments_table(db::SupportedProviders::PostgreSQL)));
            ASSERT_TRUE(result.is_success()) << "Failed to create comments table";
        }

        // Create standard test tables (users + posts)
        void CreateStandardTables() const {
            CreateUsersTable();
            CreatePostsTable();
        }

        // Create all tables (users, posts, orders, comments)
        void CreateAllTables() const {
            CreateUsersTable();
            CreatePostsTable();
            CreateOrdersTable();
            CreateCommentsTable();
        }

        // Create extended tables (for aggregate/clause tests)
        void CreateExtendedTables() const {
            CreateUsersExtendedTable();
            CreateOrdersExtendedTable();
        }

        void CreateAggregateTestTables() const {
            CreateUsersExtendedTable();
            CreateOrdersTable();
        }

        void CreateClauseTestTables() const {
            CreateUsersExtendedTable();
            CreateOrdersExtendedTable();
            CreateTestTable();
        }

        void CreateTestTable() const {
            const auto result = executor_->execute(R"(
            CREATE TABLE IF NOT EXISTS test_table (
                id SERIAL PRIMARY KEY,
                value INTEGER
            )
        )");
            ASSERT_TRUE(result.is_success()) << "Failed to create test_table";
        }

        // Drop helpers
        void DropUsersTable() const {
            (void)executor_->execute("DROP TABLE IF EXISTS users CASCADE");
        }

        void DropPostsTable() const {
            (void)executor_->execute("DROP TABLE IF EXISTS posts CASCADE");
        }

        void DropOrdersTable() const {
            (void)executor_->execute("DROP TABLE IF EXISTS orders CASCADE");
        }

        void DropCommentsTable() const {
            (void)executor_->execute("DROP TABLE IF EXISTS comments CASCADE");
        }

        void DropTestTable() const {
            (void)executor_->execute("DROP TABLE IF EXISTS test_table CASCADE");
        }

        void DropStandardTables() const {
            DropPostsTable();
            DropUsersTable();
        }

        void DropAllTables() const {
            DropCommentsTable();
            DropOrdersTable();
            DropPostsTable();
            DropUsersTable();
        }

        void DropClauseTestTables() const {
            DropTestTable();
            DropOrdersTable();
            DropUsersTable();
        }

        // Truncate helpers (faster than drop+create)
        void TruncateUsersTable() const {
            (void)executor_->execute("TRUNCATE TABLE users RESTART IDENTITY CASCADE");
        }

        void TruncatePostsTable() const {
            (void)executor_->execute("TRUNCATE TABLE posts RESTART IDENTITY CASCADE");
        }

        void TruncateOrdersTable() const {
            (void)executor_->execute("TRUNCATE TABLE orders RESTART IDENTITY CASCADE");
        }

        void TruncateCommentsTable() const {
            (void)executor_->execute("TRUNCATE TABLE comments RESTART IDENTITY CASCADE");
        }

        void TruncateStandardTables() const {
            TruncatePostsTable();
            TruncateUsersTable();
        }

        void TruncateAllTables() const {
            TruncateCommentsTable();
            TruncateOrdersTable();
            TruncatePostsTable();
            TruncateUsersTable();
        }

        // Standard test data insertion
        void InsertTestUsers() const {
            ASSERT_TRUE(executor_->execute("INSERT INTO users (id, name, age, active) VALUES (1, 'Alice', 30, true)"));
            ASSERT_TRUE(executor_->execute("INSERT INTO users (id, name, age, active) VALUES (2, 'Bob', 25, false)"));
            ASSERT_TRUE(
                executor_->execute("INSERT INTO users (id, name, age, active) VALUES (3, 'Charlie', 35, true)"));
        }

        void InsertTestPosts() const {
            ASSERT_TRUE(executor_->execute(
                "INSERT INTO posts (id, user_id, title, published) VALUES (1, 1, 'Post by Alice', true)"));
            ASSERT_TRUE(executor_->execute(
                "INSERT INTO posts (id, user_id, title, published) VALUES (2, 1, 'Another Alice Post', false)"));
            ASSERT_TRUE(executor_->execute(
                "INSERT INTO posts (id, user_id, title, published) VALUES (3, 2, 'Bob Post', true)"));
        }

        void InsertTestOrders() const {
            ASSERT_TRUE(
                executor_->execute("INSERT INTO orders (id, user_id, amount, completed) VALUES (1, 1, 100.00, true)"));
            ASSERT_TRUE(
                executor_->execute("INSERT INTO orders (id, user_id, amount, completed) VALUES (2, 1, 200.00, false)"));
            ASSERT_TRUE(
                executor_->execute("INSERT INTO orders (id, user_id, amount, completed) VALUES (3, 2, 150.00, true)"));
        }

        void InsertTestComments() const {
            ASSERT_TRUE(executor_->execute(
                "INSERT INTO comments (id, post_id, user_id, content) VALUES (1, 1, 2, 'Nice post!')"));
            ASSERT_TRUE(executor_->execute(
                "INSERT INTO comments (id, post_id, user_id, content) VALUES (2, 1, 3, 'Great work!')"));
        }

        void InsertStandardTestData() const {
            InsertTestUsers();
            InsertTestPosts();
        }

        void InsertAllTestData() const {
            InsertTestUsers();
            InsertTestPosts();
            InsertTestOrders();
            InsertTestComments();
        }

        // Extended test data for aggregate/clause tests
        void InsertExtendedTestUsers() const {
            ASSERT_TRUE(executor_->execute("INSERT INTO users (id, name, age, active, department, salary) VALUES (1, "
                                           "'Alice', 30, true, 'Engineering', 75000.00)"));
            ASSERT_TRUE(executor_->execute("INSERT INTO users (id, name, age, active, department, salary) VALUES (2, "
                                           "'Bob', 25, true, 'Engineering', 65000.00)"));
            ASSERT_TRUE(executor_->execute("INSERT INTO users (id, name, age, active, department, salary) VALUES (3, "
                                           "'Charlie', 35, false, 'Sales', 55000.00)"));
            ASSERT_TRUE(executor_->execute("INSERT INTO users (id, name, age, active, department, salary) VALUES (4, "
                                           "'Diana', 28, true, 'Sales', 60000.00)"));
            ASSERT_TRUE(executor_->execute("INSERT INTO users (id, name, age, active, department, salary) VALUES (5, "
                                           "'Eve', 32, true, 'Marketing', 70000.00)"));
        }

        void InsertAggregateTestData() const {
            InsertExtendedTestUsers();
            InsertTestOrders();
        }

        // Clause test data (extended users, orders with status, test_table)
        void InsertClauseTestData() const {
            // Insert users with department and salary for clause testing
            ASSERT_TRUE(executor_->execute("INSERT INTO users (id, name, age, active, department, salary) VALUES "
                                           "(1, 'Alice', 30, true, 'Engineering', 75000.00)"));
            ASSERT_TRUE(executor_->execute("INSERT INTO users (id, name, age, active, department, salary) VALUES "
                                           "(2, 'Bob', 25, true, 'Engineering', 65000.00)"));
            ASSERT_TRUE(executor_->execute("INSERT INTO users (id, name, age, active, department, salary) VALUES "
                                           "(3, 'Charlie', 35, false, 'Sales', 55000.00)"));
            ASSERT_TRUE(executor_->execute("INSERT INTO users (id, name, age, active, department, salary) VALUES "
                                           "(4, 'Diana', 28, true, 'Sales', 60000.00)"));
            ASSERT_TRUE(executor_->execute("INSERT INTO users (id, name, age, active, department, salary) VALUES "
                                           "(5, 'Eve', 32, true, 'Marketing', 70000.00)"));
            ASSERT_TRUE(executor_->execute("INSERT INTO users (id, name, age, active, department, salary) VALUES "
                                           "(6, 'Frank', 45, true, 'Engineering', 85000.00)"));
            ASSERT_TRUE(executor_->execute("INSERT INTO users (id, name, age, active, department, salary) VALUES "
                                           "(7, 'Grace', 22, true, 'Marketing', 50000.00)"));

            // Insert orders with status
            ASSERT_TRUE(executor_->execute("INSERT INTO orders (id, user_id, amount, completed, status) VALUES "
                                           "(1, 1, 500.00, true, 'completed')"));
            ASSERT_TRUE(executor_->execute("INSERT INTO orders (id, user_id, amount, completed, status) VALUES "
                                           "(2, 1, 300.00, true, 'completed')"));
            ASSERT_TRUE(executor_->execute("INSERT INTO orders (id, user_id, amount, completed, status) VALUES "
                                           "(3, 2, 200.00, false, 'pending')"));
            ASSERT_TRUE(executor_->execute("INSERT INTO orders (id, user_id, amount, completed, status) VALUES "
                                           "(4, 3, 150.00, true, 'completed')"));
            ASSERT_TRUE(executor_->execute("INSERT INTO orders (id, user_id, amount, completed, status) VALUES "
                                           "(5, 4, 600.00, true, 'completed')"));

            // Insert into test_table
            ASSERT_TRUE(executor_->execute("INSERT INTO test_table (id, value) VALUES (1, 100)"));
        }

        // Subquery test data (users, posts, orders relationships)
        void InsertSubqueryTestData() const {
            ASSERT_TRUE(executor_->execute("INSERT INTO users (id, name, age, active) VALUES (1, 'Alice', 30, true)"));
            ASSERT_TRUE(executor_->execute("INSERT INTO users (id, name, age, active) VALUES (2, 'Bob', 25, true)"));
            ASSERT_TRUE(
                executor_->execute("INSERT INTO users (id, name, age, active) VALUES (3, 'Charlie', 35, false)"));
            ASSERT_TRUE(executor_->execute("INSERT INTO users (id, name, age, active) VALUES (4, 'Diana', 28, true)"));
            ASSERT_TRUE(executor_->execute(
                "INSERT INTO posts (id, user_id, title, published) VALUES (1, 1, 'Alice Post 1', true)"));
            ASSERT_TRUE(executor_->execute(
                "INSERT INTO posts (id, user_id, title, published) VALUES (2, 1, 'Alice Post 2', true)"));
            ASSERT_TRUE(executor_->execute(
                "INSERT INTO posts (id, user_id, title, published) VALUES (3, 2, 'Bob Post', true)"));
            ASSERT_TRUE(executor_->execute(
                "INSERT INTO posts (id, user_id, title, published) VALUES (4, 3, 'Charlie Draft', false)"));
            ASSERT_TRUE(
                executor_->execute("INSERT INTO orders (id, user_id, amount, completed) VALUES (1, 1, 100.00, true)"));
            ASSERT_TRUE(
                executor_->execute("INSERT INTO orders (id, user_id, amount, completed) VALUES (2, 1, 500.00, true)"));
            ASSERT_TRUE(
                executor_->execute("INSERT INTO orders (id, user_id, amount, completed) VALUES (3, 2, 250.00, true)"));
            ASSERT_TRUE(
                executor_->execute("INSERT INTO orders (id, user_id, amount, completed) VALUES (4, 3, 75.00, false)"));
        }

        // Condition test data (varied ages for range testing)
        void InsertConditionTestData() const {
            ASSERT_TRUE(executor_->execute("INSERT INTO users (id, name, age, active) VALUES (1, 'john', 25, true)"));
            ASSERT_TRUE(executor_->execute("INSERT INTO users (id, name, age, active) VALUES (2, 'jane', 30, true)"));
            ASSERT_TRUE(executor_->execute("INSERT INTO users (id, name, age, active) VALUES (3, 'bob', 18, false)"));
            ASSERT_TRUE(executor_->execute("INSERT INTO users (id, name, age, active) VALUES (4, 'alice', 65, true)"));
            ASSERT_TRUE(
                executor_->execute("INSERT INTO users (id, name, age, active) VALUES (5, 'charlie', 70, false)"));
            ASSERT_TRUE(
                executor_->execute("INSERT INTO posts (id, user_id, title, published) VALUES (1, 1, 'Post 1', true)"));
            ASSERT_TRUE(
                executor_->execute("INSERT INTO posts (id, user_id, title, published) VALUES (2, 2, 'Post 2', true)"));
        }

        // Row counting helpers
        [[nodiscard]] int CountUsersRows() const {
            if (auto result = executor_->execute("SELECT COUNT(*) FROM users"); result.is_success()) {
                if (const auto& block = result.value(); block.rows() > 0) {
                    return block.get<int>(0, 0);
                }
            }
            return 0;
        }

        [[nodiscard]] int CountPostsRows() const {
            if (auto result = executor_->execute("SELECT COUNT(*) FROM posts"); result.is_success()) {
                if (const auto& block = result.value(); block.rows() > 0) {
                    return block.get<int>(0, 0);
                }
            }
            return 0;
        }

        [[nodiscard]] int CountOrdersRows() const {
            if (auto result = executor_->execute("SELECT COUNT(*) FROM orders"); result.is_success()) {
                if (const auto& block = result.value(); block.rows() > 0) {
                    return block.get<int>(0, 0);
                }
            }
            return 0;
        }

        // Accessors
        [[nodiscard]] PGconn* connection() const {
            return conn_;
        }
        [[nodiscard]] db::postgres::SyncExecutor& executor() const {
            return *executor_;
        }
        [[nodiscard]] QueryLibrary& library() const {
            return *library_;
        }
        [[nodiscard]] const TestSchemas& schemas() const {
            return library_->schemas();
        }

        PGconn* conn_ = nullptr;
        std::unique_ptr<db::postgres::SyncExecutor> executor_;
        std::unique_ptr<QueryLibrary> library_;
    };

}  // namespace demiplane::test
