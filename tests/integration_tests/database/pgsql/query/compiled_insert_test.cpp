// Compiled INSERT Query Functional Tests
// Tests query compilation + execution with SyncExecutor

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
    auto query = compile_query(
        insert_into(schemas().users).into({"name", "age", "active"}).values({std::string{"John Doe"}, 25, true}));
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Insert failed: " << result.error<ErrorContext>();
    EXPECT_EQ(CountRows(), 1);
}

TEST_F(CompiledInsertTest, InsertMultipleColumns) {
    const auto& u = schemas().users;
    auto compiled_query =
        compile_query(insert_into(u).into({"name", "age", "active"}).values({std::string{"Bob"}, 25, false}));

    auto result = executor().execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Insert failed: " << result.error<ErrorContext>();
    EXPECT_EQ(CountRows(), 1);

    auto select_result = executor().execute("SELECT name, age, active FROM users WHERE name = 'Bob'");
    ASSERT_TRUE(select_result.is_success());
    EXPECT_EQ(select_result.value().rows(), 1);
}

TEST_F(CompiledInsertTest, InsertPartialColumns) {
    const auto& u       = schemas().users;
    auto compiled_query = compile_query(insert_into(u).into({"name", "age"}).values({std::string{"Charlie"}, 35}));

    auto result = executor().execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Insert failed: " << result.error<ErrorContext>();
    EXPECT_EQ(CountRows(), 1);

    auto select_result = executor().execute("SELECT name, age FROM users WHERE name = 'Charlie'");
    ASSERT_TRUE(select_result.is_success());
    EXPECT_EQ(select_result.value().rows(), 1);
}

// ============== INSERT with Multiple Rows ==============

TEST_F(CompiledInsertTest, InsertMultipleRows) {
    auto query  = compile_query(insert_into(schemas().users)
                                   .into({"name", "age", "active"})
                                   .values({std::string{"User1"}, 25, true})
                                   .values({std::string{"User2"}, 30, false}));
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Insert failed: " << result.error<ErrorContext>();
    EXPECT_EQ(CountRows(), 2);  // InsertMultipleValues produces 2 rows
}

// ============== INSERT with Record ==============

TEST_F(CompiledInsertTest, InsertFromRecord) {
    Record test_record(schemas().users_dynamic);
    test_record["name"].set(std::string("Bob Smith"));
    test_record["age"].set(35);
    test_record["active"].set(true);
    auto query  = compile_query(insert_into(schemas().users).into({"name", "age", "active"}).values(test_record));
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Insert failed: " << result.error<ErrorContext>();
    EXPECT_EQ(CountRows(), 1);

    auto select_result = executor().execute("SELECT name FROM users WHERE name = 'Bob Smith'");
    ASSERT_TRUE(select_result.is_success());
    EXPECT_EQ(select_result.value().rows(), 1);
}

TEST_F(CompiledInsertTest, InsertBatchRecords) {
    Record record1(schemas().users_dynamic);
    record1["name"].set(std::string("User1"));
    record1["age"].set(25);
    record1["active"].set(true);

    Record record2(schemas().users_dynamic);
    record2["name"].set(std::string("User2"));
    record2["age"].set(30);
    record2["active"].set(false);

    const std::vector records = {record1, record2};
    auto query  = compile_query(insert_into(schemas().users).into({"name", "age", "active"}).batch(records));
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Batch insert failed: " << result.error<ErrorContext>();
    EXPECT_EQ(CountRows(), 2);  // InsertBatch produces 2 records
}

// ============== INSERT with Different Data Types ==============

TEST_F(CompiledInsertTest, InsertWithBoolean) {
    const auto& u       = schemas().users;
    auto compiled_query = compile_query(insert_into(u).into({"name", "active"}).values({std::string{"Helen"}, true}));

    auto result = executor().execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Insert failed: " << result.error<ErrorContext>();
    EXPECT_EQ(CountRows(), 1);

    auto select_result = executor().execute("SELECT active FROM users WHERE name = 'Helen'");
    ASSERT_TRUE(select_result.is_success());
    auto& block = select_result.value();
    EXPECT_EQ(block.rows(), 1);
}

TEST_F(CompiledInsertTest, InsertWithInteger) {
    const auto& u       = schemas().users;
    auto compiled_query = compile_query(insert_into(u).into({"name", "age"}).values({std::string{"Ivan"}, 42}));

    auto result = executor().execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Insert failed: " << result.error<ErrorContext>();
    EXPECT_EQ(CountRows(), 1);

    auto select_result = executor().execute("SELECT age FROM users WHERE name = 'Ivan'");
    ASSERT_TRUE(select_result.is_success());
    auto& block = select_result.value();
    EXPECT_EQ(block.rows(), 1);
}

TEST_F(CompiledInsertTest, InsertWithString) {
    const auto& u = schemas().users;
    auto compiled_query =
        compile_query(insert_into(u).into({"name"}).values({std::string{"Long Name With Spaces And Special Ch@rs"}}));

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
    const auto& u = schemas().users;
    auto compiled_query =
        compile_query(insert_into(u).into({"name", "age"}).values({std::string{"Julia"}, std::monostate{}}));

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
    auto query  = compile_query(insert_into("users").into({"name", "age"}).values({std::string{"Jane Doe"}, 30}));
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Insert failed: " << result.error<ErrorContext>();
    EXPECT_EQ(CountRows(), 1);
}

// ============== Large Batch INSERT ==============

TEST_F(CompiledInsertTest, InsertLargeBatch) {
    std::vector<Record> records;
    for (int i = 0; i < 100; ++i) {
        Record rec(schemas().users_dynamic);
        rec["name"].set(std::string("User") + std::to_string(i));
        rec["age"].set(20 + (i % 50));
        rec["active"].set(i % 2 == 0);
        records.push_back(rec);
    }

    auto compiled_query = compile_query(insert_into(schemas().users).into({"name", "age", "active"}).batch(records));

    auto result = executor().execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Large batch insert failed: " << result.error<ErrorContext>();
    EXPECT_EQ(CountRows(), 100);
}

// ============== INSERT Edge Cases ==============

TEST_F(CompiledInsertTest, InsertEmptyString) {
    const auto& u       = schemas().users;
    auto compiled_query = compile_query(insert_into(u).into({"name", "age"}).values({std::string{""}, 25}));

    auto result = executor().execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Insert with empty string failed: " << result.error<ErrorContext>();
    EXPECT_EQ(CountRows(), 1);
}

TEST_F(CompiledInsertTest, InsertZeroValues) {
    const auto& u       = schemas().users;
    auto compiled_query = compile_query(insert_into(u).into({"name", "age"}).values({std::string{"Zero Age"}, 0}));

    auto result = executor().execute(compiled_query);

    ASSERT_TRUE(result.is_success()) << "Insert with zero values failed: " << result.error<ErrorContext>();
    EXPECT_EQ(CountRows(), 1);
}
