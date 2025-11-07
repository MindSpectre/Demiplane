// Compiled INSERT Query Functional Tests
// Tests query compilation + execution with SyncExecutor

#include "postgres_sync_executor.hpp"

#include <gtest/gtest.h>

#include "postgres_dialect.hpp"
#include "postgres_params.hpp"
#include "query_compiler.hpp"

using namespace demiplane::db;
using namespace demiplane::db::postgres;

// Test fixture for compiled INSERT queries
class CompiledInsertTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Get connection parameters from environment or use defaults
        const char* host     = std::getenv("POSTGRES_HOST") ? std::getenv("POSTGRES_HOST") : "localhost";
        const char* port     = std::getenv("POSTGRES_PORT") ? std::getenv("POSTGRES_PORT") : "5433";
        const char* dbname   = std::getenv("POSTGRES_DB") ? std::getenv("POSTGRES_DB") : "test_db";
        const char* user     = std::getenv("POSTGRES_USER") ? std::getenv("POSTGRES_USER") : "test_user";
        const char* password = std::getenv("POSTGRES_PASSWORD") ? std::getenv("POSTGRES_PASSWORD") : "test_password";

        // Create connection string
        std::string conninfo = "host=" + std::string(host) + " port=" + std::string(port) +
                               " dbname=" + std::string(dbname) + " user=" + std::string(user) +
                               " password=" + std::string(password);

        // Connect to database
        conn_ = PQconnectdb(conninfo.c_str());

        // Check connection status
        // if (PQstatus(conn_) != CONNECTION_OK) {
        //     std::string error = PQerrorMessage(conn_);
        //     PQfinish(conn_);
        //     conn_ = nullptr;
        //     GTEST_SKIP() << "Failed to connect to PostgreSQL: " << error;
        // }

        // Create executor
        executor_ = std::make_unique<SyncExecutor>(conn_);

        // Create query compiler with PostgreSQL dialect
        compiler_ = std::make_unique<QueryCompiler>(std::make_unique<postgres::Dialect>(), false);

        // Create test schema
        users_schema_ = std::make_shared<Table>("test_users");
        users_schema_->add_field<int>("id", "SERIAL PRIMARY KEY")
            .add_field<std::string>("name", "VARCHAR(100)")
            .add_field<int>("age", "INTEGER")
            .add_field<bool>("active", "BOOLEAN");

        // Create actual table in database
        auto create_result = executor_->execute(R"(
            CREATE TABLE IF NOT EXISTS test_users (
                id SERIAL PRIMARY KEY,
                name VARCHAR(100),
                age INTEGER,
                active BOOLEAN
            )
        )");
        ASSERT_TRUE(create_result.is_success()) << "Failed to create test table: "
                                                << create_result.error<ErrorContext>();

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

    int CountRows() const {
        auto result = executor_->execute("SELECT COUNT(*) FROM test_users");
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
    std::unique_ptr<QueryCompiler> compiler_;

    std::shared_ptr<Table> users_schema_;
};

// ============== Basic INSERT Tests ==============

TEST_F(CompiledInsertTest, InsertSingleRow) {
    // Build and compile INSERT query
    auto query =
        insert_into(users_schema_).into({"name", "age", "active"}).values({std::string{"Alice"}, 30, true});
    auto compiled_query = compiler_->compile(query);
    // Execute compiled query
    auto result = executor_->execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Insert failed: " << result.error<ErrorContext>();

    // Verify insertion
    EXPECT_EQ(CountRows(), 1);
}

TEST_F(CompiledInsertTest, InsertMultipleColumns) {
    // Build and compile INSERT query with multiple columns
    auto query = insert_into(users_schema_)
                     .into({"name", "age", "active"})
                     .values({std::string{"Bob"}, 25, false});
    auto compiled_query = compiler_->compile(query);

    // Execute compiled query
    auto result = executor_->execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Insert failed: " << result.error<ErrorContext>();
    EXPECT_EQ(CountRows(), 1);

    // Verify data
    auto select_result = executor_->execute("SELECT name, age, active FROM test_users WHERE name = 'Bob'");
    ASSERT_TRUE(select_result.is_success());
    EXPECT_EQ(select_result.value().rows(), 1);
}

TEST_F(CompiledInsertTest, InsertPartialColumns) {
    // Build and compile INSERT query with only some columns
    auto query = insert_into(users_schema_).into({"name", "age"}).values({std::string{"Charlie"}, 35});
    auto compiled_query = compiler_->compile(query);

    // Execute compiled query
    auto result = executor_->execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Insert failed: " << result.error<ErrorContext>();
    EXPECT_EQ(CountRows(), 1);

    // Verify data (active should be NULL)
    auto select_result = executor_->execute("SELECT name, age FROM test_users WHERE name = 'Charlie'");
    ASSERT_TRUE(select_result.is_success());
    EXPECT_EQ(select_result.value().rows(), 1);
}

// ============== INSERT with Multiple Rows ==============

TEST_F(CompiledInsertTest, InsertMultipleRows) {
    // Build and compile INSERT query with multiple rows
    auto query = insert_into(users_schema_)
                     .into({"name", "age", "active"})
                     .values({std::string{"User1"}, 20, true})
                     .values({std::string{"User2"}, 25, false})
                     .values({std::string{"User3"}, 30, true});
    auto compiled_query = compiler_->compile(query);

    // Execute compiled query
    auto result = executor_->execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Insert failed: " << result.error<ErrorContext>();

    // Verify insertions
    EXPECT_EQ(CountRows(), 3);
}

// ============== INSERT with Record ==============

TEST_F(CompiledInsertTest, InsertFromRecord) {
    // Create a Record
    Record test_record(users_schema_);
    test_record["name"].set(std::string("Dave"));
    test_record["age"].set(40);
    test_record["active"].set(true);

    // Build and compile INSERT query from Record
    auto query          = insert_into(users_schema_).into({"name", "age", "active"}).values(test_record);
    auto compiled_query = compiler_->compile(query);

    // Execute compiled query
    auto result = executor_->execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Insert failed: " << result.error<ErrorContext>();
    EXPECT_EQ(CountRows(), 1);

    // Verify data
    auto select_result = executor_->execute("SELECT name FROM test_users WHERE name = 'Dave'");
    ASSERT_TRUE(select_result.is_success());
    EXPECT_EQ(select_result.value().rows(), 1);
}

TEST_F(CompiledInsertTest, InsertBatchRecords) {
    // Create multiple Records
    Record record1(users_schema_);
    record1["name"].set(std::string("Eve"));
    record1["age"].set(28);
    record1["active"].set(true);

    Record record2(users_schema_);
    record2["name"].set(std::string("Frank"));
    record2["age"].set(32);
    record2["active"].set(false);

    Record record3(users_schema_);
    record3["name"].set(std::string("Grace"));
    record3["age"].set(27);
    record3["active"].set(true);

    std::vector<Record> records = {record1, record2, record3};

    // Build and compile batch INSERT query
    auto query          = insert_into(users_schema_).into({"name", "age", "active"}).batch(records);
    auto compiled_query = compiler_->compile(query);

    // Execute compiled query
    auto result = executor_->execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Batch insert failed: " << result.error<ErrorContext>();

    // Verify insertions
    EXPECT_EQ(CountRows(), 3);
}

// ============== INSERT with Different Data Types ==============

TEST_F(CompiledInsertTest, InsertWithBoolean) {
    // Build and compile INSERT with boolean values
    auto query = insert_into(users_schema_).into({"name", "active"}).values({std::string{"Helen"}, true});
    auto compiled_query = compiler_->compile(query);

    // Execute compiled query
    auto result = executor_->execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Insert failed: " << result.error<ErrorContext>();
    EXPECT_EQ(CountRows(), 1);

    // Verify boolean value
    auto select_result = executor_->execute("SELECT active FROM test_users WHERE name = 'Helen'");
    ASSERT_TRUE(select_result.is_success());
    auto& block = select_result.value();
    EXPECT_EQ(block.rows(), 1);
}

TEST_F(CompiledInsertTest, InsertWithInteger) {
    // Build and compile INSERT with integer values
    auto query = insert_into(users_schema_).into({"name", "age"}).values({std::string{"Ivan"}, 42});
    auto compiled_query = compiler_->compile(query);

    // Execute compiled query
    auto result = executor_->execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Insert failed: " << result.error<ErrorContext>();
    EXPECT_EQ(CountRows(), 1);

    // Verify integer value
    auto select_result = executor_->execute("SELECT age FROM test_users WHERE name = 'Ivan'");
    ASSERT_TRUE(select_result.is_success());
    auto& block = select_result.value();
    EXPECT_EQ(block.rows(), 1);
}

TEST_F(CompiledInsertTest, InsertWithString) {
    // Build and compile INSERT with string values
    auto query =
        insert_into(users_schema_).into({"name"}).values({std::string{"Long Name With Spaces And Special Ch@rs"}});
    auto compiled_query = compiler_->compile(query);

    // Execute compiled query
    auto result = executor_->execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Insert failed: " << result.error<ErrorContext>();
    EXPECT_EQ(CountRows(), 1);

    // Verify string value
    auto select_result =
        executor_->execute("SELECT name FROM test_users WHERE name = 'Long Name With Spaces And Special Ch@rs'");
    ASSERT_TRUE(select_result.is_success());
    EXPECT_EQ(select_result.value().rows(), 1);
}

// ============== INSERT with NULL Values ==============

TEST_F(CompiledInsertTest, InsertWithNullAge) {
    // Build and compile INSERT with NULL value
    auto query =
        insert_into(users_schema_).into({"name", "age"}).values({std::string{"Julia"}, std::monostate{}});
    auto compiled_query = compiler_->compile(query);

    // Execute compiled query
    auto result = executor_->execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Insert with NULL failed: " << result.error<ErrorContext>();
    EXPECT_EQ(CountRows(), 1);

    // Verify NULL was inserted
    auto select_result = executor_->execute("SELECT age FROM test_users WHERE name = 'Julia'");
    ASSERT_TRUE(select_result.is_success());
    auto& block = select_result.value();
    EXPECT_EQ(block.rows(), 1);
    auto age_opt = block.get_opt<int>(0, 0);
    EXPECT_FALSE(age_opt.has_value()) << "Age should be NULL";
}

// ============== INSERT with Table Name String ==============

TEST_F(CompiledInsertTest, InsertWithTableName) {
    // Build and compile INSERT using table name string
    auto query = insert_into("test_users").into({"name", "age"}).values({std::string{"Kevin"}, 33});
    auto compiled_query = compiler_->compile(query);

    // Execute compiled query
    auto result = executor_->execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Insert failed: " << result.error<ErrorContext>();
    EXPECT_EQ(CountRows(), 1);
}

// ============== Large Batch INSERT ==============

TEST_F(CompiledInsertTest, InsertLargeBatch) {
    // Create a large batch of records
    std::vector<Record> records;
    for (int i = 0; i < 100; ++i) {
        Record rec(users_schema_);
        rec["name"].set(std::string("User") + std::to_string(i));
        rec["age"].set(20 + (i % 50));
        rec["active"].set(i % 2 == 0);
        records.push_back(rec);
    }

    // Build and compile batch INSERT query
    auto query          = insert_into(users_schema_).into({"name", "age", "active"}).batch(records);
    auto compiled_query = compiler_->compile(query);

    // Execute compiled query
    auto result = executor_->execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Large batch insert failed: " << result.error<ErrorContext>();

    // Verify insertions
    EXPECT_EQ(CountRows(), 100);
}

// ============== INSERT Edge Cases ==============

TEST_F(CompiledInsertTest, InsertEmptyString) {
    // Build and compile INSERT with empty string
    auto query = insert_into(users_schema_).into({"name", "age"}).values({std::string{""}, 25});
    auto compiled_query = compiler_->compile(query);

    // Execute compiled query
    auto result = executor_->execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Insert with empty string failed: " << result.error<ErrorContext>();
    EXPECT_EQ(CountRows(), 1);
}

TEST_F(CompiledInsertTest, InsertZeroValues) {
    // Build and compile INSERT with zero values
    auto query = insert_into(users_schema_).into({"name", "age"}).values({std::string{"Zero Age"}, 0});
    auto compiled_query = compiler_->compile(query);

    // Execute compiled query
    auto result = executor_->execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Insert with zero values failed: " << result.error<ErrorContext>();
    EXPECT_EQ(CountRows(), 1);
}
