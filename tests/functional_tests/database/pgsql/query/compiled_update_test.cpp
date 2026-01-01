// Compiled UPDATE Query Functional Tests
// Tests query compilation + execution with SyncExecutor

#include <demiplane/nexus>
#include <demiplane/scroll>

#include <gtest/gtest.h>

#include "postgres_dialect.hpp"
#include "postgres_params.hpp"
#include "postgres_sync_executor.hpp"
#include "query_compiler.hpp"


using namespace demiplane::db::postgres;

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
        // Get connection parameters from environment or use defaults
        const char* host     = std::getenv("POSTGRES_HOST") ? std::getenv("POSTGRES_HOST") : "localhost";
        const char* port     = std::getenv("POSTGRES_PORT") ? std::getenv("POSTGRES_PORT") : "5433";
        const char* dbname   = std::getenv("POSTGRES_DB") ? std::getenv("POSTGRES_DB") : "test_db";
        const char* user     = std::getenv("POSTGRES_USER") ? std::getenv("POSTGRES_USER") : "test_user";
        const char* password = std::getenv("POSTGRES_PASSWORD") ? std::getenv("POSTGRES_PASSWORD") : "test_password";

        // Create connection string
        std::string conn_info = "host=" + std::string(host) + " port=" + std::string(port) +
                                " dbname=" + std::string(dbname) + " user=" + std::string(user) +
                                " password=" + std::string(password);

        // Connect to database
        conn_ = PQconnectdb(conn_info.c_str());

        // Check connection status
        if (PQstatus(conn_) != CONNECTION_OK) {
            const std::string error = PQerrorMessage(conn_);
            PQfinish(conn_);
            conn_ = nullptr;
            GTEST_SKIP() << "Failed to connect to PostgreSQL: " << error;
        }

        // Create executor
        executor_ = std::make_unique<SyncExecutor>(conn_);

        // Create query compiler with PostgreSQL dialect
        compiler_ = std::make_unique<demiplane::db::QueryCompiler>(std::make_unique<Dialect>(), false);

        // Create test schema
        users_schema_ = std::make_shared<demiplane::db::Table>("test_users");
        users_schema_->add_field<int>("id", "SERIAL PRIMARY KEY")
            .add_field<std::string>("name", "VARCHAR(100)")
            .add_field<int>("age", "INTEGER")
            .add_field<bool>("active", "BOOLEAN");

        // Create column references
        user_id_     = users_schema_->column<int>("id");
        user_name_   = users_schema_->column<std::string>("name");
        user_age_    = users_schema_->column<int>("age");
        user_active_ = users_schema_->column<bool>("active");

        // Create actual table in database
        auto create_result = executor_->execute(R"(
            CREATE TABLE IF NOT EXISTS test_users (
                id SERIAL PRIMARY KEY,
                name VARCHAR(100),
                age INTEGER,
                active BOOLEAN
            )
        )");
        ASSERT_TRUE(create_result.is_success())
            << "Failed to create test table: " << create_result.error<ErrorContext>();

        // Clean table
        CleanTestTable();
    }

    void TearDown() override {
        if (conn_) {
            // Drop test table
            EXPECT_TRUE(executor_->execute("DROP TABLE IF EXISTS test_users CASCADE"));
            PQfinish(conn_);
            conn_ = nullptr;
        }
    }

    void CleanTestTable() const {
        auto result = executor_->execute("TRUNCATE TABLE test_users RESTART IDENTITY CASCADE");
        ASSERT_TRUE(result.is_success()) << "Failed to clean test table: " << result.error<ErrorContext>();
    }

    PGconn* conn_{nullptr};
    std::unique_ptr<SyncExecutor> executor_;
    std::unique_ptr<demiplane::db::QueryCompiler> compiler_;

    std::shared_ptr<demiplane::db::Table> users_schema_;
    demiplane::db::TableColumn<int> user_id_{nullptr, ""};
    demiplane::db::TableColumn<std::string> user_name_{nullptr, ""};
    demiplane::db::TableColumn<int> user_age_{nullptr, ""};
    demiplane::db::TableColumn<bool> user_active_{nullptr, ""};
};
using namespace demiplane::db;
// ============== Basic UPDATE Tests ==============

TEST_F(CompiledUpdateTest, UpdateSingleColumn) {
    // Insert test data
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age, active) VALUES ('Alice', 30, true)"));

    // Build and compile UPDATE query
    auto query          = update(users_schema_).set("age", 31).where(user_name_ == std::string{"Alice"});
    auto compiled_query = compiler_->compile(query);

    // Execute compiled query
    auto result = executor_->execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Update failed: " << result.error<ErrorContext>();

    // Verify update
    auto select_result = executor_->execute("SELECT age FROM test_users WHERE name = 'Alice'");
    ASSERT_TRUE(select_result.is_success());
    auto& block = select_result.value();
    EXPECT_EQ(block.rows(), 1);
    EXPECT_EQ(block.get<int>(0, 0), 31);
}

TEST_F(CompiledUpdateTest, UpdateMultipleColumns) {
    // Insert test data
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age, active) VALUES ('Bob', 25, false)"));

    // Build and compile UPDATE query with multiple columns
    auto query = update(users_schema_).set("age", 26).set("active", true).where(user_name_ == std::string{"Bob"});
    auto compiled_query = compiler_->compile(query);

    // Execute compiled query
    auto result = executor_->execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Update failed: " << result.error<ErrorContext>();

    // Verify update
    auto select_result = executor_->execute("SELECT age, active FROM test_users WHERE name = 'Bob'");
    ASSERT_TRUE(select_result.is_success());
    auto& block = select_result.value();
    EXPECT_EQ(block.rows(), 1);
    EXPECT_EQ(block.get<int>(0, 0), 26);
    EXPECT_EQ(block.get<bool>(0, 1), true);
}

TEST_F(CompiledUpdateTest, UpdateWithInitializerList) {
    // Insert test data
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age, active) VALUES ('Charlie', 35, true)"));

    // Build and compile UPDATE query with initializer list
    auto query = update(users_schema_)
                     .set({
                         {"age",    36   },
                         {"active", false}
    })
                     .where(user_name_ == std::string{"Charlie"});
    auto compiled_query = compiler_->compile(query);

    // Execute compiled query
    auto result = executor_->execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Update failed: " << result.error<ErrorContext>();

    // Verify update
    auto select_result = executor_->execute("SELECT age, active FROM test_users WHERE name = 'Charlie'");
    ASSERT_TRUE(select_result.is_success());
    auto& block = select_result.value();
    EXPECT_EQ(block.rows(), 1);
    EXPECT_EQ(block.get<int>(0, 0), 36);
    EXPECT_EQ(block.get<bool>(0, 1), false);
}

// ============== UPDATE with WHERE Conditions ==============

TEST_F(CompiledUpdateTest, UpdateWithSimpleWhere) {
    // Insert test data
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age, active) VALUES ('User1', 20, true)"));
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age, active) VALUES ('User2', 30, true)"));
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age, active) VALUES ('User3', 40, true)"));

    // Build and compile UPDATE query with WHERE condition
    auto query          = update(users_schema_).set("active", false).where(user_age_ > 25);
    auto compiled_query = compiler_->compile(query);

    // Execute compiled query
    auto result = executor_->execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Update failed: " << result.error<ErrorContext>();

    // Verify update - only User2 and User3 should be updated
    auto select_result = executor_->execute("SELECT COUNT(*) FROM test_users WHERE active = false");
    ASSERT_TRUE(select_result.is_success());
    EXPECT_EQ(select_result.value().get<int>(0, 0), 2);
}

TEST_F(CompiledUpdateTest, UpdateWithComplexWhere) {
    // Insert test data
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age, active) VALUES ('User1', 25, true)"));
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age, active) VALUES ('User2', 30, true)"));
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age, active) VALUES ('User3', 35, false)"));

    // Build and compile UPDATE query with complex WHERE (AND condition)
    auto query          = update(users_schema_).set("age", 40).where((user_age_ >= 25) && (user_active_ == true));
    auto compiled_query = compiler_->compile(query);

    // Execute compiled query
    auto result = executor_->execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Update failed: " << result.error<ErrorContext>();

    // Verify update - only User1 and User2 should be updated
    auto select_result = executor_->execute("SELECT COUNT(*) FROM test_users WHERE age = 40");
    ASSERT_TRUE(select_result.is_success());
    EXPECT_EQ(select_result.value().get<int>(0, 0), 2);
}

TEST_F(CompiledUpdateTest, UpdateWithOrCondition) {
    // Insert test data
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age, active) VALUES ('User1', 20, true)"));
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age, active) VALUES ('User2', 30, false)"));
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age, active) VALUES ('User3', 40, true)"));

    // Build and compile UPDATE query with OR condition
    auto query          = update(users_schema_).set("age", 50).where((user_age_ < 25) || (user_age_ > 35));
    auto compiled_query = compiler_->compile(query);

    // Execute compiled query
    auto result = executor_->execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Update failed: " << result.error<ErrorContext>();

    // Verify update - User1 and User3 should be updated
    auto select_result = executor_->execute("SELECT COUNT(*) FROM test_users WHERE age = 50");
    ASSERT_TRUE(select_result.is_success());
    EXPECT_EQ(select_result.value().get<int>(0, 0), 2);
}

// ============== UPDATE without WHERE (all rows) ==============

TEST_F(CompiledUpdateTest, UpdateAllRows) {
    // Insert test data
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age, active) VALUES ('User1', 25, true)"));
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age, active) VALUES ('User2', 30, false)"));
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age, active) VALUES ('User3', 35, true)"));

    // Build and compile UPDATE query without WHERE (updates all rows)
    auto query          = update(users_schema_).set("active", false);
    auto compiled_query = compiler_->compile(query);

    // Execute compiled query
    auto result = executor_->execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Update failed: " << result.error<ErrorContext>();

    // Verify all rows are updated
    auto select_result = executor_->execute("SELECT COUNT(*) FROM test_users WHERE active = false");
    ASSERT_TRUE(select_result.is_success());
    EXPECT_EQ(select_result.value().get<int>(0, 0), 3);
}

// ============== UPDATE with Different Data Types ==============

TEST_F(CompiledUpdateTest, UpdateString) {
    // Insert test data
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age) VALUES ('OldName', 30)"));

    // Build and compile UPDATE query with string
    auto query          = update(users_schema_).set("name", std::string{"NewName"}).where(user_age_ == 30);
    auto compiled_query = compiler_->compile(query);

    // Execute compiled query
    auto result = executor_->execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Update failed: " << result.error<ErrorContext>();

    // Verify update
    auto select_result = executor_->execute("SELECT name FROM test_users WHERE age = 30");
    ASSERT_TRUE(select_result.is_success());
    auto& block = select_result.value();
    EXPECT_EQ(block.rows(), 1);
    EXPECT_EQ(block.get<std::string>(0, 0), "NewName");
}

TEST_F(CompiledUpdateTest, UpdateBoolean) {
    // Insert test data
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, active) VALUES ('TestUser', true)"));

    // Build and compile UPDATE query with boolean
    auto query          = update(users_schema_).set("active", false).where(user_name_ == std::string{"TestUser"});
    auto compiled_query = compiler_->compile(query);

    // Execute compiled query
    auto result = executor_->execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Update failed: " << result.error<ErrorContext>();

    // Verify update
    auto select_result = executor_->execute("SELECT active FROM test_users WHERE name = 'TestUser'");
    ASSERT_TRUE(select_result.is_success());
    auto& block = select_result.value();
    EXPECT_EQ(block.rows(), 1);
    EXPECT_EQ(block.get<bool>(0, 0), false);
}

TEST_F(CompiledUpdateTest, UpdateInteger) {
    // Insert test data
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age) VALUES ('TestUser', 25)"));

    // Build and compile UPDATE query with integer
    auto query          = update(users_schema_).set("age", 50).where(user_name_ == std::string{"TestUser"});
    auto compiled_query = compiler_->compile(query);

    // Execute compiled query
    auto result = executor_->execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Update failed: " << result.error<ErrorContext>();

    // Verify update
    auto select_result = executor_->execute("SELECT age FROM test_users WHERE name = 'TestUser'");
    ASSERT_TRUE(select_result.is_success());
    auto& block = select_result.value();
    EXPECT_EQ(block.rows(), 1);
    EXPECT_EQ(block.get<int>(0, 0), 50);
}

// ============== UPDATE with NULL Values ==============

TEST_F(CompiledUpdateTest, UpdateToNull) {
    // Insert test data
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age) VALUES ('TestUser', 30)"));

    // Build and compile UPDATE query setting to NULL
    auto query = update(users_schema_).set("age", std::monostate{}).where(user_name_ == std::string{"TestUser"});
    auto compiled_query = compiler_->compile(query);

    // Execute compiled query
    auto result = executor_->execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Update to NULL failed: " << result.error<ErrorContext>();

    // Verify NULL was set
    auto select_result = executor_->execute("SELECT age FROM test_users WHERE name = 'TestUser'");
    ASSERT_TRUE(select_result.is_success());
    auto& block = select_result.value();
    EXPECT_EQ(block.rows(), 1);
    auto age_opt = block.get_opt<int>(0, 0);
    EXPECT_FALSE(age_opt.has_value()) << "Age should be NULL";
}

// ============== UPDATE with Table Name String ==============

TEST_F(CompiledUpdateTest, UpdateWithTableName) {
    // Insert test data
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age) VALUES ('TestUser', 25)"));

    // Build and compile UPDATE query using table name string
    auto query          = update("test_users").set("age", 35).where(user_name_ == std::string{"TestUser"});
    auto compiled_query = compiler_->compile(query);

    // Execute compiled query
    auto result = executor_->execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Update failed: " << result.error<ErrorContext>();

    // Verify update
    auto select_result = executor_->execute("SELECT age FROM test_users WHERE name = 'TestUser'");
    ASSERT_TRUE(select_result.is_success());
    EXPECT_EQ(select_result.value().get<int>(0, 0), 35);
}

// ============== UPDATE Edge Cases ==============

TEST_F(CompiledUpdateTest, UpdateNoMatch) {
    // Insert test data
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age) VALUES ('TestUser', 25)"));

    // Build and compile UPDATE query that matches no rows
    auto query          = update(users_schema_).set("age", 50).where(user_age_ > 100);
    auto compiled_query = compiler_->compile(query);

    // Execute compiled query
    auto result = executor_->execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Update failed: " << result.error<ErrorContext>();

    // Verify no rows were updated
    auto select_result = executor_->execute("SELECT age FROM test_users WHERE name = 'TestUser'");
    ASSERT_TRUE(select_result.is_success());
    EXPECT_EQ(select_result.value().get<int>(0, 0), 25);  // Original value
}

TEST_F(CompiledUpdateTest, UpdateEmptyTable) {
    // Don't insert any data

    // Build and compile UPDATE query on empty table
    auto query          = update(users_schema_).set("active", false);
    auto compiled_query = compiler_->compile(query);

    // Execute compiled query
    auto result = executor_->execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Update failed: " << result.error<ErrorContext>();

    // Verify table is still empty
    auto select_result = executor_->execute("SELECT COUNT(*) FROM test_users");
    ASSERT_TRUE(select_result.is_success());
    EXPECT_EQ(select_result.value().get<int>(0, 0), 0);
}

TEST_F(CompiledUpdateTest, UpdateToSameValue) {
    // Insert test data
    EXPECT_TRUE(executor_->execute("INSERT INTO test_users (name, age) VALUES ('TestUser', 30)"));

    // Build and compile UPDATE query setting to same value
    auto query          = update(users_schema_).set("age", 30).where(user_name_ == std::string{"TestUser"});
    auto compiled_query = compiler_->compile(query);

    // Execute compiled query
    auto result = executor_->execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Update failed: " << result.error<ErrorContext>();

    // Verify value is still the same
    auto select_result = executor_->execute("SELECT age FROM test_users WHERE name = 'TestUser'");
    ASSERT_TRUE(select_result.is_success());
    EXPECT_EQ(select_result.value().get<int>(0, 0), 30);
}
