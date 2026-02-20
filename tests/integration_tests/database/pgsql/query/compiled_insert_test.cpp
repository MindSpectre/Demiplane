// Compiled INSERT Query Functional Tests
// Tests query compilation + execution with SyncExecutor using QueryLibrary

#include <test_fixture.hpp>

using namespace demiplane::db;
using namespace demiplane::db::postgres;
using namespace demiplane::test;

// Test fixture for compiled INSERT queries
class CompiledInsertTest : public PgsqlTestFixture {
protected:
    void SetUp() override {
        PgsqlTestFixture::SetUp();
        if (connection() == nullptr)
            return;
        CreateUsersTable();
        TruncateUsersTable();
    }

    void TearDown() override {
        if (connection()) {
            DropUsersTable();
        }
        PgsqlTestFixture::TearDown();
    }

    [[nodiscard]] int CountRows() const {
        return CountUsersRows();
    }
};

// ============== Basic INSERT Tests ==============

TEST_F(CompiledInsertTest, InsertSingleRow) {
    auto query  = library().produce<ins::BasicInsert>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Insert failed: " << result.error<ErrorContext>();
    EXPECT_EQ(CountRows(), 1);
}

TEST_F(CompiledInsertTest, InsertMultipleColumns) {
    const auto& s       = library().schemas().users();
    auto query          = insert_into(s.table).into({"name", "age", "active"}).values({std::string{"Bob"}, 25, false});
    auto compiled_query = library().compiler().compile(query);

    auto result = executor().execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Insert failed: " << result.error<ErrorContext>();
    EXPECT_EQ(CountRows(), 1);

    auto select_result = executor().execute("SELECT name, age, active FROM users WHERE name = 'Bob'");
    ASSERT_TRUE(select_result.is_success());
    EXPECT_EQ(select_result.value().rows(), 1);
}

TEST_F(CompiledInsertTest, InsertPartialColumns) {
    const auto& s       = library().schemas().users();
    auto query          = insert_into(s.table).into({"name", "age"}).values({std::string{"Charlie"}, 35});
    auto compiled_query = library().compiler().compile(query);

    auto result = executor().execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Insert failed: " << result.error<ErrorContext>();
    EXPECT_EQ(CountRows(), 1);

    auto select_result = executor().execute("SELECT name, age FROM users WHERE name = 'Charlie'");
    ASSERT_TRUE(select_result.is_success());
    EXPECT_EQ(select_result.value().rows(), 1);
}

// ============== INSERT with Multiple Rows ==============

TEST_F(CompiledInsertTest, InsertMultipleRows) {
    auto query  = library().produce<ins::InsertMultipleValues>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Insert failed: " << result.error<ErrorContext>();
    EXPECT_EQ(CountRows(), 2);  // InsertMultipleValues produces 2 rows
}

// ============== INSERT with Record ==============

TEST_F(CompiledInsertTest, InsertFromRecord) {
    auto query  = library().produce<ins::InsertWithRecord>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Insert failed: " << result.error<ErrorContext>();
    EXPECT_EQ(CountRows(), 1);

    auto select_result = executor().execute("SELECT name FROM users WHERE name = 'Bob Smith'");
    ASSERT_TRUE(select_result.is_success());
    EXPECT_EQ(select_result.value().rows(), 1);
}

TEST_F(CompiledInsertTest, InsertBatchRecords) {
    auto query  = library().produce<ins::InsertBatch>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Batch insert failed: " << result.error<ErrorContext>();
    EXPECT_EQ(CountRows(), 2);  // InsertBatch produces 2 records
}

// ============== INSERT with Different Data Types ==============

TEST_F(CompiledInsertTest, InsertWithBoolean) {
    const auto& s       = library().schemas().users();
    auto query          = insert_into(s.table).into({"name", "active"}).values({std::string{"Helen"}, true});
    auto compiled_query = library().compiler().compile(query);

    auto result = executor().execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Insert failed: " << result.error<ErrorContext>();
    EXPECT_EQ(CountRows(), 1);

    auto select_result = executor().execute("SELECT active FROM users WHERE name = 'Helen'");
    ASSERT_TRUE(select_result.is_success());
    auto& block = select_result.value();
    EXPECT_EQ(block.rows(), 1);
}

TEST_F(CompiledInsertTest, InsertWithInteger) {
    const auto& s       = library().schemas().users();
    auto query          = insert_into(s.table).into({"name", "age"}).values({std::string{"Ivan"}, 42});
    auto compiled_query = library().compiler().compile(query);

    auto result = executor().execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Insert failed: " << result.error<ErrorContext>();
    EXPECT_EQ(CountRows(), 1);

    auto select_result = executor().execute("SELECT age FROM users WHERE name = 'Ivan'");
    ASSERT_TRUE(select_result.is_success());
    auto& block = select_result.value();
    EXPECT_EQ(block.rows(), 1);
}

TEST_F(CompiledInsertTest, InsertWithString) {
    const auto& s = library().schemas().users();
    auto query = insert_into(s.table).into({"name"}).values({std::string{"Long Name With Spaces And Special Ch@rs"}});
    auto compiled_query = library().compiler().compile(query);

    auto result = executor().execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Insert failed: " << result.error<ErrorContext>();
    EXPECT_EQ(CountRows(), 1);

    auto select_result =
        executor().execute("SELECT name FROM users WHERE name = 'Long Name With Spaces And Special Ch@rs'");
    ASSERT_TRUE(select_result.is_success());
    EXPECT_EQ(select_result.value().rows(), 1);
}

// ============== INSERT with NULL Values ==============

TEST_F(CompiledInsertTest, InsertWithNullAge) {
    const auto& s       = library().schemas().users();
    auto query          = insert_into(s.table).into({"name", "age"}).values({std::string{"Julia"}, std::monostate{}});
    auto compiled_query = library().compiler().compile(query);

    auto result = executor().execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Insert with NULL failed: " << result.error<ErrorContext>();
    EXPECT_EQ(CountRows(), 1);

    auto select_result = executor().execute("SELECT age FROM users WHERE name = 'Julia'");
    ASSERT_TRUE(select_result.is_success());
    auto& block = select_result.value();
    EXPECT_EQ(block.rows(), 1);
    auto age_opt = block.get_opt<int>(0, 0);
    EXPECT_FALSE(age_opt.has_value()) << "Age should be NULL";
}

// ============== INSERT with Table Name String ==============

TEST_F(CompiledInsertTest, InsertWithTableName) {
    auto query  = library().produce<ins::InsertWithTableName>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Insert failed: " << result.error<ErrorContext>();
    EXPECT_EQ(CountRows(), 1);
}

// ============== Large Batch INSERT ==============

TEST_F(CompiledInsertTest, InsertLargeBatch) {
    const auto& s = library().schemas().users();

    std::vector<Record> records;
    for (int i = 0; i < 100; ++i) {
        Record rec(s.table);
        rec["name"].set(std::string("User") + std::to_string(i));
        rec["age"].set(20 + (i % 50));
        rec["active"].set(i % 2 == 0);
        records.push_back(rec);
    }

    auto query          = insert_into(s.table).into({"name", "age", "active"}).batch(records);
    auto compiled_query = library().compiler().compile(query);

    auto result = executor().execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Large batch insert failed: " << result.error<ErrorContext>();
    EXPECT_EQ(CountRows(), 100);
}

// ============== INSERT Edge Cases ==============

TEST_F(CompiledInsertTest, InsertEmptyString) {
    const auto& s       = library().schemas().users();
    auto query          = insert_into(s.table).into({"name", "age"}).values({std::string{""}, 25});
    auto compiled_query = library().compiler().compile(query);

    auto result = executor().execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Insert with empty string failed: " << result.error<ErrorContext>();
    EXPECT_EQ(CountRows(), 1);
}

TEST_F(CompiledInsertTest, InsertZeroValues) {
    const auto& s       = library().schemas().users();
    auto query          = insert_into(s.table).into({"name", "age"}).values({std::string{"Zero Age"}, 0});
    auto compiled_query = library().compiler().compile(query);

    auto result = executor().execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Insert with zero values failed: " << result.error<ErrorContext>();
    EXPECT_EQ(CountRows(), 1);
}
