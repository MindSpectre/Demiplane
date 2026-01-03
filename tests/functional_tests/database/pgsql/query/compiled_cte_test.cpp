// Compiled CTE (Common Table Expression) Query Functional Tests
// Tests query compilation + execution with SyncExecutor using QueryLibrary

#include <test_fixture.hpp>

using namespace demiplane::db;
using namespace demiplane::db::postgres;
using namespace demiplane::test;

class CompiledCteTest : public PgsqlTestFixture {
protected:
    void SetUp() override {
        PgsqlTestFixture::SetUp();
        if (connection() == nullptr) return;
        CreateAllTables();
        InsertAllTestData();
    }

    void TearDown() override {
        if (connection()) {
            DropAllTables();
        }
        PgsqlTestFixture::TearDown();
    }
};

// ============== Basic CTE Tests ==============

TEST_F(CompiledCteTest, BasicCte) {
    auto query  = library().produce<cte::BasicCte>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    // Should return active users (id, name)
    EXPECT_GE(block.cols(), 2);
}

TEST_F(CompiledCteTest, CteWithSelect) {
    auto query  = library().produce<cte::CteWithSelect>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    // Should return user_id, total_amount
    EXPECT_GE(block.cols(), 2);
}

TEST_F(CompiledCteTest, CteWithJoin) {
    auto query  = library().produce<cte::CteWithJoin>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    // Should return published posts (id, title, user_id)
    EXPECT_GE(block.cols(), 3);
}

TEST_F(CompiledCteTest, MultipleCtes) {
    auto query  = library().produce<cte::MultipleCtes>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    // Should return user_id, post_count
    EXPECT_GE(block.cols(), 2);
}

TEST_F(CompiledCteTest, CteWithAggregates) {
    auto query  = library().produce<cte::CteWithAggregates>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    // Should return user_id, order_count, total_spent, avg_order
    EXPECT_GE(block.cols(), 4);
}
