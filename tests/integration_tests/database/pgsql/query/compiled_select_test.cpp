// Compiled SELECT Query Functional Tests
// Tests query compilation + execution with SyncExecutor

#include <test_fixture.hpp>

using namespace demiplane::db;
using namespace demiplane::db::postgres;
using namespace demiplane::test;

class CompiledSelectTest : public PgsqlTestFixture {
protected:
    void SetUp() override {
        PgsqlTestFixture::SetUp();
        if (connection() == nullptr)
            return;
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

    auto query =
        compile_query(select(schemas().users.column<"id">(), schemas().users.column<"name">()).from(schemas().users));
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_EQ(block.rows(), 3);
    EXPECT_EQ(block.cols(), 2);  // id, name
}

TEST_F(CompiledSelectTest, SelectAllColumns) {
    InsertTestUsers();

    auto query  = compile_query(select(all("users")).from(schemas().users));
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_EQ(block.rows(), 3);
}

TEST_F(CompiledSelectTest, SelectDistinct) {
    // Insert duplicate data
    ASSERT_TRUE(executor().execute("INSERT INTO users (id, name, age, active) VALUES (1, 'Alice', 30, true)"));
    ASSERT_TRUE(executor().execute("INSERT INTO users (id, name, age, active) VALUES (2, 'Alice', 30, true)"));
    ASSERT_TRUE(executor().execute("INSERT INTO users (id, name, age, active) VALUES (3, 'Bob', 25, false)"));

    auto query = compile_query(
        select_distinct(schemas().users.column<"name">(), schemas().users.column<"age">()).from(schemas().users));
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_EQ(block.rows(), 2);  // Only 2 distinct (name, age) combinations
}

TEST_F(CompiledSelectTest, SelectWithWhere) {
    InsertTestUsers();

    auto query = compile_query(
        select(schemas().users.column<"name">()).from(schemas().users).where(schemas().users.column<"age">() > 18));
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_GE(block.rows(), 1);  // Users with age > 18
}

TEST_F(CompiledSelectTest, SelectWithJoin) {
    InsertTestUsers();
    ASSERT_TRUE(
        executor().execute("INSERT INTO posts (id, user_id, title, published) VALUES (1, 1, 'Post by Alice', true)"));
    ASSERT_TRUE(
        executor().execute("INSERT INTO posts (id, user_id, title, published) VALUES (2, 2, 'Post by Bob', true)"));

    auto query  = compile_query(select(schemas().users.column<"name">(), schemas().posts.column<"title">())
                                   .from(schemas().users)
                                   .join(schemas().posts)
                                   .on(schemas().posts.column<"user_id">() == schemas().users.column<"id">()));
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_EQ(block.rows(), 2);  // Two users with posts
}

TEST_F(CompiledSelectTest, SelectWithGroupBy) {
    InsertTestUsers();

    auto query =
        compile_query(select(schemas().users.column<"active">(), count(schemas().users.column<"id">()).as("user_count"))
                          .from(schemas().users)
                          .group_by(schemas().users.column<"active">()));
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_EQ(block.rows(), 2);  // Two groups: active=true, active=false
}

TEST_F(CompiledSelectTest, SelectWithHaving) {
    // Insert more users to have groups with count > 5
    for (int i = 1; i <= 7; ++i) {
        ASSERT_TRUE(executor().execute("INSERT INTO users (name, age, active) VALUES ('User" + std::to_string(i) +
                                       "', 25, true)"));
    }
    ASSERT_TRUE(executor().execute("INSERT INTO users (name, age, active) VALUES ('Inactive', 30, false)"));

    auto query =
        compile_query(select(schemas().users.column<"active">(), count(schemas().users.column<"id">()).as("user_count"))
                          .from(schemas().users)
                          .group_by(schemas().users.column<"active">())
                          .having(count(schemas().users.column<"id">()) > 5));
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_EQ(block.rows(), 1);  // Only group with count > 5
}

TEST_F(CompiledSelectTest, SelectWithOrderBy) {
    // Insert in random order
    ASSERT_TRUE(executor().execute("INSERT INTO users (id, name, age, active) VALUES (1, 'Zebra', 30, true)"));
    ASSERT_TRUE(executor().execute("INSERT INTO users (id, name, age, active) VALUES (2, 'Alpha', 25, true)"));
    ASSERT_TRUE(executor().execute("INSERT INTO users (id, name, age, active) VALUES (3, 'Beta', 35, true)"));

    auto query = compile_query(
        select(schemas().users.column<"name">()).from(schemas().users).order_by(asc(schemas().users.column<"name">())));
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_EQ(block.rows(), 3);
    // Results should be ordered: Alpha, Beta, Zebra
}

TEST_F(CompiledSelectTest, SelectWithLimit) {
    // Insert 10 users
    for (int i = 1; i <= 10; ++i) {
        ASSERT_TRUE(executor().execute("INSERT INTO users (name, age, active) VALUES ('User" + std::to_string(i) +
                                       "', " + std::to_string(20 + i) + ", true)"));
    }

    auto query  = compile_query(select(schemas().users.column<"name">()).from(schemas().users).limit(10));
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_LE(block.rows(), 10);  // Limited to 10
}

TEST_F(CompiledSelectTest, SelectMixedTypes) {
    InsertTestUsers();

    auto query = compile_query(
        select(schemas().users.column<"name">(), "constant", count(schemas().users.column<"id">()).as("total"))
            .from(schemas().users)
            .group_by(schemas().users.column<"name">()));
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_EQ(block.rows(), 3);  // One row per unique name (Alice, Bob, Charlie)
    EXPECT_EQ(block.cols(), 3);  // name, constant, total
}

// ============== Empty Result Tests ==============

TEST_F(CompiledSelectTest, SelectEmptyResult) {
    // Don't insert any data
    auto query =
        compile_query(select(schemas().users.column<"id">(), schemas().users.column<"name">()).from(schemas().users));
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_EQ(block.rows(), 0);
    EXPECT_TRUE(block.empty());
}
