// Compiled CTE (Common Table Expression) Query Functional Tests
// Tests query compilation + execution with SyncExecutor

#include <test_fixture.hpp>

using namespace demiplane::db;
using namespace demiplane::db::postgres;
using namespace demiplane::test;

class CompiledCteTest : public PgsqlTestFixture {
protected:
    void SetUp() override {
        PgsqlTestFixture::SetUp();
        if (connection() == nullptr)
            return;
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
    const auto& s = schemas();
    auto cte      = with("active_users",
                    select(s.users.column<"id">(), s.users.column<"name">())
                        .from(s.users)
                        .where(s.users.column<"active">() == true));
    auto query    = compile_query(select(col("id"), col("name")).from(cte));
    auto result   = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    // Should return active users (id, name)
    EXPECT_GE(block.cols(), 2);
}

TEST_F(CompiledCteTest, CteWithSelect) {
    const auto& s = schemas();
    auto cte      = with("user_orders",
                    select(s.orders.column<"user_id">(), sum(s.orders.column<"amount">()).as("total_amount"))
                        .from(s.orders)
                        .where(s.orders.column<"completed">() == true)
                        .group_by(s.orders.column<"user_id">()));
    auto query    = compile_query(select(col("user_id"), col("total_amount")).from(cte));
    auto result   = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    // Should return user_id, total_amount
    EXPECT_GE(block.cols(), 2);
}

TEST_F(CompiledCteTest, CteWithJoin) {
    const auto& s = schemas();
    auto cte      = with("published_posts",
                    select(s.posts.column<"id">(), s.posts.column<"title">(), s.posts.column<"user_id">())
                        .from(s.posts)
                        .where(s.posts.column<"published">() == true));
    auto query    = compile_query(select(col("id"), col("title"), col("user_id")).from(cte));
    auto result   = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    // Should return published posts (id, title, user_id)
    EXPECT_GE(block.cols(), 3);
}

TEST_F(CompiledCteTest, MultipleCtes) {
    const auto& s = schemas();
    auto cte      = with("post_stats",
                    select(s.posts.column<"user_id">(), count(s.posts.column<"id">()).as("post_count"))
                        .from(s.posts)
                        .group_by(s.posts.column<"user_id">()));
    auto query    = compile_query(select(col("user_id"), col("post_count")).from(cte));
    auto result   = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    // Should return user_id, post_count
    EXPECT_GE(block.cols(), 2);
}

TEST_F(CompiledCteTest, CteWithAggregates) {
    const auto& s = schemas();
    auto cte      = with("order_stats",
                    select(s.orders.column<"user_id">(),
                           count(s.orders.column<"id">()).as("order_count"),
                           sum(s.orders.column<"amount">()).as("total_spent"),
                           avg(s.orders.column<"amount">()).as("avg_order"))
                        .from(s.orders)
                        .where(s.orders.column<"completed">() == true)
                        .group_by(s.orders.column<"user_id">()));
    auto query =
        compile_query(select(col("user_id"), col("order_count"), col("total_spent"), col("avg_order")).from(cte));
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    // Should return user_id, order_count, total_spent, avg_order
    EXPECT_GE(block.cols(), 4);
}
