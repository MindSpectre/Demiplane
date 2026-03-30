// Compiled SUBQUERY Query Functional Tests
// Tests query compilation + execution with SyncExecutor

#include <test_fixture.hpp>

using namespace demiplane::db;
using namespace demiplane::db::postgres;
using namespace demiplane::test;

class CompiledSubqueryTest : public PgsqlTestFixture {
protected:
    void SetUp() override {
        PgsqlTestFixture::SetUp();
        if (connection() == nullptr)
            return;
        CreateUsersTable();
        CreatePostsTable();
        CreateOrdersTable();
        InsertSubqueryTestData();
    }

    void TearDown() override {
        if (connection()) {
            DropOrdersTable();
            DropPostsTable();
            DropUsersTable();
        }
        PgsqlTestFixture::TearDown();
    }
};

// ============== Subquery in WHERE Tests ==============

TEST_F(CompiledSubqueryTest, SubqueryInWhere) {
    const auto& s     = schemas();
    auto active_users = select(s.users.column<"id">()).from(s.users).where(s.users.column<"active">() == true);
    auto query        = compile_query(
        select(s.posts.column<"title">()).from(s.posts).where(in(s.posts.column<"user_id">(), subquery(active_users))));
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    // Should return posts by active users
    EXPECT_GE(block.rows(), 1);
}

// ============== EXISTS Tests ==============

TEST_F(CompiledSubqueryTest, Exists) {
    const auto& s        = schemas();
    auto published_posts = select(1).from(s.posts).where(s.posts.column<"user_id">() == s.users.column<"id">() &&
                                                         s.posts.column<"published">() == true);
    auto query           = compile_query(select(s.users.column<"name">()).from(s.users).where(exists(published_posts)));
    auto result          = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    // Should return users who have published posts
    EXPECT_GE(block.rows(), 1);
}

TEST_F(CompiledSubqueryTest, NotExists) {
    const auto& s       = schemas();
    auto pending_orders = select(1).from(s.orders).where(s.orders.column<"user_id">() == s.users.column<"id">() &&
                                                         s.orders.column<"completed">() == false);
    auto query          = compile_query(select(s.users.column<"name">()).from(s.users).where(!exists(pending_orders)));
    auto result         = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
}

// ============== IN Subquery Tests ==============

TEST_F(CompiledSubqueryTest, InSubqueryMultiple) {
    const auto& s         = schemas();
    auto high_value_users = select(s.users.column<"id">())
                                .from(s.orders)
                                .where(s.orders.column<"amount">() > lit(1000.0))
                                .group_by(s.orders.column<"user_id">())
                                .having(sum(s.orders.column<"amount">()) > lit(5000.0));
    auto query = compile_query(
        select(s.users.column<"name">()).from(s.users).where(in(s.users.column<"id">(), subquery(high_value_users))));
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
}

// ============== Nested Subquery Tests ==============

TEST_F(CompiledSubqueryTest, NestedSubqueries) {
    const auto& s = schemas();
    auto users_with_completed_orders =
        select(s.orders.column<"user_id">()).from(s.orders).where(s.orders.column<"completed">() == true);
    auto posts_by_active_users = select(s.posts.column<"user_id">())
                                     .from(s.posts)
                                     .where(in(s.posts.column<"user_id">(), subquery(users_with_completed_orders)));
    auto query  = compile_query(select(s.users.column<"name">())
                                   .from(s.users)
                                   .where(in(s.users.column<"id">(), subquery(posts_by_active_users))));
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
}

// ============== Subquery with Aggregates ==============

TEST_F(CompiledSubqueryTest, SubqueryWithAggregates) {
    const auto& s = schemas();
    auto avg_order_amount =
        select(avg(s.orders.column<"amount">())).from(s.orders).where(s.orders.column<"completed">() == true);
    auto query  = compile_query(select(s.users.column<"name">(), s.orders.column<"amount">())
                                   .from(s.users)
                                   .join(s.orders)
                                   .on(s.orders.column<"user_id">() == s.users.column<"id">())
                                   .where(s.orders.column<"amount">() > subquery(avg_order_amount)));
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
}

// ============== Subquery with DISTINCT ==============

TEST_F(CompiledSubqueryTest, SubqueryWithDistinct) {
    const auto& s = schemas();
    auto unique_publishers =
        select_distinct(s.posts.column<"user_id">()).from(s.posts).where(s.posts.column<"published">() == true);
    auto query = compile_query(
        select(s.users.column<"name">()).from(s.users).where(in(s.users.column<"id">(), subquery(unique_publishers))));
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
}
