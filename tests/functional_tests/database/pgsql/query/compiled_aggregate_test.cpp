// Compiled AGGREGATE Query Functional Tests
// Tests query compilation + execution with SyncExecutor using QueryLibrary

#include <test_fixture.hpp>

using namespace demiplane::db;
using namespace demiplane::db::postgres;
using namespace demiplane::test;

class CompiledAggregateTest : public PgsqlTestFixture {
protected:
    void SetUp() override {
        PgsqlTestFixture::SetUp();
        if (connection() == nullptr) return;
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
    auto query  = library().produce<aggregate::Count>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_EQ(block.rows(), 1);
    EXPECT_EQ(block.cols(), 1);
}

TEST_F(CompiledAggregateTest, Sum) {
    auto query  = library().produce<aggregate::Sum>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_EQ(block.rows(), 1);
}

TEST_F(CompiledAggregateTest, Avg) {
    auto query  = library().produce<aggregate::Avg>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_EQ(block.rows(), 1);
}

TEST_F(CompiledAggregateTest, Min) {
    auto query  = library().produce<aggregate::Min>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_EQ(block.rows(), 1);
}

TEST_F(CompiledAggregateTest, Max) {
    auto query  = library().produce<aggregate::Max>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_EQ(block.rows(), 1);
}

// ============== Advanced Aggregate Tests ==============

TEST_F(CompiledAggregateTest, AggregateWithAlias) {
    auto query  = library().produce<aggregate::AggregateWithAlias>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
}

TEST_F(CompiledAggregateTest, CountDistinct) {
    auto query  = library().produce<aggregate::CountDistinct>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
}

TEST_F(CompiledAggregateTest, CountAll) {
    auto query  = library().produce<aggregate::CountAll>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_EQ(block.rows(), 1);
}

TEST_F(CompiledAggregateTest, AggregateGroupBy) {
    auto query  = library().produce<aggregate::AggregateGroupBy>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    // Should have groups for Engineering, Sales, Marketing
    EXPECT_GE(block.rows(), 1);
}

TEST_F(CompiledAggregateTest, AggregateHaving) {
    auto query  = library().produce<aggregate::AggregateHaving>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
}

TEST_F(CompiledAggregateTest, MultipleAggregates) {
    auto query  = library().produce<aggregate::MultipleAggregates>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    // Multiple aggregate columns
    EXPECT_GE(block.cols(), 2);
}

TEST_F(CompiledAggregateTest, AggregateMixedTypes) {
    auto query  = library().produce<aggregate::AggregateMixedTypes>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
}
