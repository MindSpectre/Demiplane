// Compiled SELECT Query Functional Tests
// Tests query compilation + execution with SyncExecutor using QueryLibrary

#include <test_fixture.hpp>

using namespace demiplane::db;
using namespace demiplane::db::postgres;
using namespace demiplane::test;

class CompiledSelectTest : public PgsqlTestFixture {
protected:
    void SetUp() override {
        PgsqlTestFixture::SetUp();
        if (connection() == nullptr) return;
        CreateStandardTables();
        TruncateStandardTables();
    }

    void TearDown() override {
        if (connection()) {
            DropStandardTables();
        }
        PgsqlTestFixture::TearDown();
    }
};

// ============== Basic SELECT Tests ==============

TEST_F(CompiledSelectTest, BasicSelect) {
    InsertTestUsers();

    auto query  = library().produce<sel::BasicSelect>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_EQ(block.rows(), 3);
    EXPECT_EQ(block.cols(), 2);  // id, name
}

TEST_F(CompiledSelectTest, SelectAllColumns) {
    InsertTestUsers();

    auto query  = library().produce<sel::SelectAllColumns>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_EQ(block.rows(), 3);
}

TEST_F(CompiledSelectTest, SelectDistinct) {
    // Insert duplicate data
    ASSERT_TRUE(executor().execute(
        "INSERT INTO users (id, name, age, active) VALUES (1, 'Alice', 30, true)"));
    ASSERT_TRUE(executor().execute(
        "INSERT INTO users (id, name, age, active) VALUES (2, 'Alice', 30, true)"));
    ASSERT_TRUE(executor().execute(
        "INSERT INTO users (id, name, age, active) VALUES (3, 'Bob', 25, false)"));

    auto query  = library().produce<sel::SelectDistinct>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_EQ(block.rows(), 2);  // Only 2 distinct (name, age) combinations
}

TEST_F(CompiledSelectTest, SelectWithWhere) {
    InsertTestUsers();

    auto query  = library().produce<sel::SelectWithWhere>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_GE(block.rows(), 1);  // Users with age > 18
}

TEST_F(CompiledSelectTest, SelectWithJoin) {
    InsertTestUsers();
    ASSERT_TRUE(executor().execute(
        "INSERT INTO posts (id, user_id, title, published) VALUES (1, 1, 'Post by Alice', true)"));
    ASSERT_TRUE(executor().execute(
        "INSERT INTO posts (id, user_id, title, published) VALUES (2, 2, 'Post by Bob', true)"));

    auto query  = library().produce<sel::SelectWithJoin>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_EQ(block.rows(), 2);  // Two users with posts
}

TEST_F(CompiledSelectTest, SelectWithGroupBy) {
    InsertTestUsers();

    auto query  = library().produce<sel::SelectWithGroupBy>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_EQ(block.rows(), 2);  // Two groups: active=true, active=false
}

TEST_F(CompiledSelectTest, SelectWithHaving) {
    // Insert more users to have groups with count > 5
    for (int i = 1; i <= 7; ++i) {
        ASSERT_TRUE(executor().execute(
            "INSERT INTO users (name, age, active) VALUES ('User" + std::to_string(i) + "', 25, true)"));
    }
    ASSERT_TRUE(executor().execute(
        "INSERT INTO users (name, age, active) VALUES ('Inactive', 30, false)"));

    auto query  = library().produce<sel::SelectWithHaving>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_EQ(block.rows(), 1);  // Only group with count > 5
}

TEST_F(CompiledSelectTest, SelectWithOrderBy) {
    // Insert in random order
    ASSERT_TRUE(executor().execute(
        "INSERT INTO users (id, name, age, active) VALUES (1, 'Zebra', 30, true)"));
    ASSERT_TRUE(executor().execute(
        "INSERT INTO users (id, name, age, active) VALUES (2, 'Alpha', 25, true)"));
    ASSERT_TRUE(executor().execute(
        "INSERT INTO users (id, name, age, active) VALUES (3, 'Beta', 35, true)"));

    auto query  = library().produce<sel::SelectWithOrderBy>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_EQ(block.rows(), 3);
    // Results should be ordered: Alpha, Beta, Zebra
}

TEST_F(CompiledSelectTest, SelectWithLimit) {
    // Insert 10 users
    for (int i = 1; i <= 10; ++i) {
        ASSERT_TRUE(executor().execute(
            "INSERT INTO users (name, age, active) VALUES ('User" + std::to_string(i) + "', " +
            std::to_string(20 + i) + ", true)"));
    }

    auto query  = library().produce<sel::SelectWithLimit>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_LE(block.rows(), 10);  // Limited to 10
}

TEST_F(CompiledSelectTest, SelectMixedTypes) {
    InsertTestUsers();

    auto query  = library().produce<sel::SelectMixedTypes>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_EQ(block.rows(), 3);  // One row per unique name (Alice, Bob, Charlie)
    EXPECT_EQ(block.cols(), 3);  // name, constant, total
}

// ============== Empty Result Tests ==============

TEST_F(CompiledSelectTest, SelectEmptyResult) {
    // Don't insert any data
    auto query  = library().produce<sel::BasicSelect>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_EQ(block.rows(), 0);
    EXPECT_TRUE(block.empty());
}
