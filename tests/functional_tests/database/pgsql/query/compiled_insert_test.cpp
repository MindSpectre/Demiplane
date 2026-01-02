// Compiled INSERT Query Functional Tests
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

// Test fixture for compiled INSERT queries
class CompiledInsertTest : public ::testing::Test {
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
        ASSERT_TRUE(create_result.is_success()) << "Failed to create table: " << create_result.error<ErrorContext>();
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
        if (auto result = executor_->execute("SELECT COUNT(*) FROM users"); result.is_success()) {
            if (const auto& block = result.value(); block.rows() > 0) {
                return block.get<int>(0, 0);
            }
        }
        return 0;
    }

    PGconn* conn_{nullptr};
    std::unique_ptr<SyncExecutor> executor_;
    std::unique_ptr<QueryLibrary> library_;
};

// ============== Basic INSERT Tests ==============

TEST_F(CompiledInsertTest, InsertSingleRow) {
    auto query = library_->produce<ins::BasicInsert>();
    auto result = executor_->execute(query);

    ASSERT_TRUE(result.is_success()) << "Insert failed: " << result.error<ErrorContext>();
    EXPECT_EQ(CountRows(), 1);
}

TEST_F(CompiledInsertTest, InsertMultipleColumns) {
    const auto& s = library_->schemas().users();
    auto query = insert_into(s.table)
                     .into({"name", "age", "active"})
                     .values({std::string{"Bob"}, 25, false});
    auto compiled_query = library_->compiler().compile(query);

    auto result = executor_->execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Insert failed: " << result.error<ErrorContext>();
    EXPECT_EQ(CountRows(), 1);

    auto select_result = executor_->execute("SELECT name, age, active FROM users WHERE name = 'Bob'");
    ASSERT_TRUE(select_result.is_success());
    EXPECT_EQ(select_result.value().rows(), 1);
}

TEST_F(CompiledInsertTest, InsertPartialColumns) {
    const auto& s = library_->schemas().users();
    auto query = insert_into(s.table).into({"name", "age"}).values({std::string{"Charlie"}, 35});
    auto compiled_query = library_->compiler().compile(query);

    auto result = executor_->execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Insert failed: " << result.error<ErrorContext>();
    EXPECT_EQ(CountRows(), 1);

    auto select_result = executor_->execute("SELECT name, age FROM users WHERE name = 'Charlie'");
    ASSERT_TRUE(select_result.is_success());
    EXPECT_EQ(select_result.value().rows(), 1);
}

// ============== INSERT with Multiple Rows ==============

TEST_F(CompiledInsertTest, InsertMultipleRows) {
    auto query = library_->produce<ins::InsertMultipleValues>();
    auto result = executor_->execute(query);

    ASSERT_TRUE(result.is_success()) << "Insert failed: " << result.error<ErrorContext>();
    EXPECT_EQ(CountRows(), 2);  // InsertMultipleValues produces 2 rows
}

// ============== INSERT with Record ==============

TEST_F(CompiledInsertTest, InsertFromRecord) {
    auto query = library_->produce<ins::InsertWithRecord>();
    auto result = executor_->execute(query);

    ASSERT_TRUE(result.is_success()) << "Insert failed: " << result.error<ErrorContext>();
    EXPECT_EQ(CountRows(), 1);

    auto select_result = executor_->execute("SELECT name FROM users WHERE name = 'Bob Smith'");
    ASSERT_TRUE(select_result.is_success());
    EXPECT_EQ(select_result.value().rows(), 1);
}

TEST_F(CompiledInsertTest, InsertBatchRecords) {
    auto query = library_->produce<ins::InsertBatch>();
    auto result = executor_->execute(query);

    ASSERT_TRUE(result.is_success()) << "Batch insert failed: " << result.error<ErrorContext>();
    EXPECT_EQ(CountRows(), 2);  // InsertBatch produces 2 records
}

// ============== INSERT with Different Data Types ==============

TEST_F(CompiledInsertTest, InsertWithBoolean) {
    const auto& s = library_->schemas().users();
    auto query = insert_into(s.table).into({"name", "active"}).values({std::string{"Helen"}, true});
    auto compiled_query = library_->compiler().compile(query);

    auto result = executor_->execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Insert failed: " << result.error<ErrorContext>();
    EXPECT_EQ(CountRows(), 1);

    auto select_result = executor_->execute("SELECT active FROM users WHERE name = 'Helen'");
    ASSERT_TRUE(select_result.is_success());
    auto& block = select_result.value();
    EXPECT_EQ(block.rows(), 1);
}

TEST_F(CompiledInsertTest, InsertWithInteger) {
    const auto& s = library_->schemas().users();
    auto query = insert_into(s.table).into({"name", "age"}).values({std::string{"Ivan"}, 42});
    auto compiled_query = library_->compiler().compile(query);

    auto result = executor_->execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Insert failed: " << result.error<ErrorContext>();
    EXPECT_EQ(CountRows(), 1);

    auto select_result = executor_->execute("SELECT age FROM users WHERE name = 'Ivan'");
    ASSERT_TRUE(select_result.is_success());
    auto& block = select_result.value();
    EXPECT_EQ(block.rows(), 1);
}

TEST_F(CompiledInsertTest, InsertWithString) {
    const auto& s = library_->schemas().users();
    auto query =
        insert_into(s.table).into({"name"}).values({std::string{"Long Name With Spaces And Special Ch@rs"}});
    auto compiled_query = library_->compiler().compile(query);

    auto result = executor_->execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Insert failed: " << result.error<ErrorContext>();
    EXPECT_EQ(CountRows(), 1);

    auto select_result =
        executor_->execute("SELECT name FROM users WHERE name = 'Long Name With Spaces And Special Ch@rs'");
    ASSERT_TRUE(select_result.is_success());
    EXPECT_EQ(select_result.value().rows(), 1);
}

// ============== INSERT with NULL Values ==============

TEST_F(CompiledInsertTest, InsertWithNullAge) {
    const auto& s = library_->schemas().users();
    auto query =
        insert_into(s.table).into({"name", "age"}).values({std::string{"Julia"}, std::monostate{}});
    auto compiled_query = library_->compiler().compile(query);

    auto result = executor_->execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Insert with NULL failed: " << result.error<ErrorContext>();
    EXPECT_EQ(CountRows(), 1);

    auto select_result = executor_->execute("SELECT age FROM users WHERE name = 'Julia'");
    ASSERT_TRUE(select_result.is_success());
    auto& block = select_result.value();
    EXPECT_EQ(block.rows(), 1);
    auto age_opt = block.get_opt<int>(0, 0);
    EXPECT_FALSE(age_opt.has_value()) << "Age should be NULL";
}

// ============== INSERT with Table Name String ==============

TEST_F(CompiledInsertTest, InsertWithTableName) {
    auto query = library_->produce<ins::InsertWithTableName>();
    auto result = executor_->execute(query);

    ASSERT_TRUE(result.is_success()) << "Insert failed: " << result.error<ErrorContext>();
    EXPECT_EQ(CountRows(), 1);
}

// ============== Large Batch INSERT ==============

TEST_F(CompiledInsertTest, InsertLargeBatch) {
    const auto& s = library_->schemas().users();

    std::vector<Record> records;
    for (int i = 0; i < 100; ++i) {
        Record rec(s.table);
        rec["name"].set(std::string("User") + std::to_string(i));
        rec["age"].set(20 + (i % 50));
        rec["active"].set(i % 2 == 0);
        records.push_back(rec);
    }

    auto query          = insert_into(s.table).into({"name", "age", "active"}).batch(records);
    auto compiled_query = library_->compiler().compile(query);

    auto result = executor_->execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Large batch insert failed: " << result.error<ErrorContext>();
    EXPECT_EQ(CountRows(), 100);
}

// ============== INSERT Edge Cases ==============

TEST_F(CompiledInsertTest, InsertEmptyString) {
    const auto& s = library_->schemas().users();
    auto query = insert_into(s.table).into({"name", "age"}).values({std::string{""}, 25});
    auto compiled_query = library_->compiler().compile(query);

    auto result = executor_->execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Insert with empty string failed: " << result.error<ErrorContext>();
    EXPECT_EQ(CountRows(), 1);
}

TEST_F(CompiledInsertTest, InsertZeroValues) {
    const auto& s = library_->schemas().users();
    auto query = insert_into(s.table).into({"name", "age"}).values({std::string{"Zero Age"}, 0});
    auto compiled_query = library_->compiler().compile(query);

    auto result = executor_->execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Insert with zero values failed: " << result.error<ErrorContext>();
    EXPECT_EQ(CountRows(), 1);
}
