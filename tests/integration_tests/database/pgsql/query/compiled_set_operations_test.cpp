// Compiled SET Operations Query Functional Tests
// Tests query compilation + execution with SyncExecutor

#include <test_fixture.hpp>

using namespace demiplane::db;
using namespace demiplane::db::postgres;
using namespace demiplane::test;

class CompiledSetOperationsTest : public PgsqlTestFixture {
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

// ============== UNION Tests ==============

TEST_F(CompiledSetOperationsTest, UnionBasic) {
    const auto& s     = schemas();
    auto active_users = select(s.users.column<"name">()).from(s.users).where(s.users.column<"active">() == true);
    auto young_users  = select(s.users.column<"name">()).from(s.users).where(s.users.column<"age">() < lit(30));
    auto query        = compile_query(union_query(active_users, young_users));
    auto result       = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    // UNION removes duplicates
    EXPECT_GE(block.rows(), 1);
}

TEST_F(CompiledSetOperationsTest, UnionAll) {
    const auto& s = schemas();
    auto completed_orders =
        select(s.orders.column<"user_id">()).from(s.orders).where(s.orders.column<"completed">() == true);
    auto pending_orders =
        select(s.orders.column<"user_id">()).from(s.orders).where(s.orders.column<"completed">() == false);
    auto query  = compile_query(union_all(completed_orders, pending_orders));
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    // UNION ALL keeps all rows including duplicates
    EXPECT_GE(block.rows(), 1);
}

// ============== INTERSECT Tests ==============

TEST_F(CompiledSetOperationsTest, Intersect) {
    const auto& s          = schemas();
    auto active_users      = select(s.users.column<"id">()).from(s.users).where(s.users.column<"active">() == true);
    auto users_with_orders = select(s.orders.column<"user_id">()).from(s.orders);
    auto query             = compile_query(intersect(active_users, users_with_orders));
    auto result            = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    // Returns only rows that appear in both result sets
}

// ============== EXCEPT Tests ==============

TEST_F(CompiledSetOperationsTest, Except) {
    const auto& s         = schemas();
    auto all_users        = select(s.users.column<"id">()).from(s.users);
    auto users_with_posts = select(s.posts.column<"user_id">()).from(s.posts);
    auto query            = compile_query(except(all_users, users_with_posts));
    auto result           = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    // Returns rows in first set but not in second
}

// ============== Combined SET Operations ==============

TEST_F(CompiledSetOperationsTest, UnionWithOrderBy) {
    const auto& s     = schemas();
    auto active_users = select(s.users.column<"name">(), s.users.column<"age">())
                            .from(s.users)
                            .where(s.users.column<"active">() == true);
    auto senior_users = select(s.users.column<"name">(), s.users.column<"age">())
                            .from(s.users)
                            .where(s.users.column<"age">() > lit(50));
    auto query  = compile_query(union_query(active_users, senior_users).order_by(desc(col("age"))));
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_GE(block.rows(), 1);
}

TEST_F(CompiledSetOperationsTest, UnionWithLimit) {
    const auto& s     = schemas();
    auto small_orders = select(s.orders.column<"id">(), s.orders.column<"amount">())
                            .from(s.orders)
                            .where(s.orders.column<"amount">() < lit(100.0));
    auto large_orders = select(s.orders.column<"id">(), s.orders.column<"amount">())
                            .from(s.orders)
                            .where(s.orders.column<"amount">() > lit(500.0));
    auto query  = compile_query(union_query(small_orders, large_orders).limit(10));
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_LE(block.rows(), 10);  // Limited to 10 rows
}

TEST_F(CompiledSetOperationsTest, MultipleUnions) {
    const auto& s = schemas();
    auto young    = select(s.users.column<"name">()).from(s.users).where(s.users.column<"age">() < lit(25));
    auto middle   = select(s.users.column<"name">())
                      .from(s.users)
                      .where(s.users.column<"age">() >= lit(25) && s.users.column<"age">() < lit(50));
    auto senior = select(s.users.column<"name">()).from(s.users).where(s.users.column<"age">() >= lit(50));
    auto query  = compile_query(union_query(union_query(young, middle), senior));
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
}

TEST_F(CompiledSetOperationsTest, MixedSetOps) {
    const auto& s = schemas();
    auto active   = select(s.users.column<"id">()).from(s.users).where(s.users.column<"active">() == true);
    auto with_orders =
        select(s.orders.column<"user_id">()).from(s.orders).where(s.orders.column<"completed">() == true);
    auto with_posts = select(s.posts.column<"user_id">()).from(s.posts).where(s.posts.column<"published">() == true);
    auto query      = compile_query(except(union_query(active, with_orders), with_posts));
    auto result     = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
}
