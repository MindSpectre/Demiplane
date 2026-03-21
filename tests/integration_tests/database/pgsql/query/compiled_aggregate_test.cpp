// Compiled AGGREGATE Query Functional Tests
// Tests query compilation + execution with SyncExecutor

#include <test_fixture.hpp>

using namespace demiplane::db;
using namespace demiplane::db::postgres;
using namespace demiplane::test;

class CompiledAggregateTest : public PgsqlTestFixture {
protected:
    void SetUp() override {
        PgsqlTestFixture::SetUp();
        if (connection() == nullptr)
            return;
        CreateAggregateTestTables();
        InsertAggregateTestData();
    }

    void TearDown() override {
        if (connection()) {
            DropOrdersTable();
            DropUsersTable();
        }
        PgsqlTestFixture::TearDown();
    }
};

// ============== Basic Aggregate Tests ==============

TEST_F(CompiledAggregateTest, Count) {
    auto query  = compile_query(select(count(schemas().users.column<"id">())).from(schemas().users));
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_EQ(block.rows(), 1);
    EXPECT_EQ(block.cols(), 1);
}

TEST_F(CompiledAggregateTest, Sum) {
    auto query  = compile_query(select(sum(schemas().users.column<"age">())).from(schemas().users));
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_EQ(block.rows(), 1);
}

TEST_F(CompiledAggregateTest, Avg) {
    auto query  = compile_query(select(avg(schemas().users.column<"age">())).from(schemas().users));
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_EQ(block.rows(), 1);
}

TEST_F(CompiledAggregateTest, Min) {
    auto query  = compile_query(select(min(schemas().users.column<"age">())).from(schemas().users));
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_EQ(block.rows(), 1);
}

TEST_F(CompiledAggregateTest, Max) {
    auto query  = compile_query(select(max(schemas().users.column<"age">())).from(schemas().users));
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_EQ(block.rows(), 1);
}

// ============== Advanced Aggregate Tests ==============

TEST_F(CompiledAggregateTest, AggregateWithAlias) {
    auto query  = compile_query(select(count(schemas().users.column<"id">()).as("total_users"),
                                      sum(schemas().users.column<"age">()).as("total_age"),
                                      avg(schemas().users.column<"age">()).as("avg_age"),
                                      min(schemas().users.column<"age">()).as("min_age"),
                                      max(schemas().users.column<"age">()).as("max_age"))
                                   .from(schemas().users));
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
}

TEST_F(CompiledAggregateTest, CountDistinct) {
    auto query  = compile_query(select(count_distinct(schemas().users.column<"age">())).from(schemas().users));
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
}

TEST_F(CompiledAggregateTest, CountAll) {
    auto query  = compile_query(select(count_all()).from(schemas().users));
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_EQ(block.rows(), 1);
}

TEST_F(CompiledAggregateTest, AggregateGroupBy) {
    auto query =
        compile_query(select(schemas().users.column<"active">(), count(schemas().users.column<"id">()).as("user_count"))
            .from(schemas().users)
            .group_by(schemas().users.column<"active">()));
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    // Should have groups for Engineering, Sales, Marketing
    EXPECT_GE(block.rows(), 1);
}

TEST_F(CompiledAggregateTest, AggregateHaving) {
    auto query =
        compile_query(select(schemas().users.column<"active">(), count(schemas().users.column<"id">()).as("user_count"))
                          .from(schemas().users)
                          .group_by(schemas().users.column<"active">())
            .having(count(schemas().users.column<"id">()) > 5));
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
}

TEST_F(CompiledAggregateTest, MultipleAggregates) {
    auto query  = compile_query(select(count(schemas().users.column<"id">()),
                                      sum(schemas().users.column<"age">()),
                                      avg(schemas().users.column<"age">()),
                                      min(schemas().users.column<"age">()),
                                      max(schemas().users.column<"age">()),
               count_distinct(schemas().users.column<"name">()))
            .from(schemas().users));
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    // Multiple aggregate columns
    EXPECT_GE(block.cols(), 2);
}

TEST_F(CompiledAggregateTest, AggregateMixedTypes) {
    auto query  = compile_query(select(schemas().users.column<"name">(),
                                      count(schemas().users.column<"id">()).as("count"),
               "literal_value",
               avg(schemas().users.column<"age">()).as("avg_age"))
            .from(schemas().users)
            .group_by(schemas().users.column<"name">()));
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
}
