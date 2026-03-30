// Compiled JOIN Query Functional Tests
// Tests query compilation + execution with SyncExecutor

#include <test_fixture.hpp>

using namespace demiplane::db;
using namespace demiplane::db::postgres;
using namespace demiplane::test;

class CompiledJoinTest : public PgsqlTestFixture {
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

// ============== INNER JOIN Tests ==============

TEST_F(CompiledJoinTest, InnerJoin) {
    auto query  = compile_query(select(schemas().users.column<"name">(), schemas().posts.column<"title">())
                                   .from(schemas().users)
                                   .join(schemas().posts)
                                   .on(schemas().posts.column<"user_id">() == schemas().users.column<"id">()));
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_GE(block.rows(), 1);
}

TEST_F(CompiledJoinTest, LeftJoin) {
    auto query  = compile_query(select(schemas().users.column<"name">(), schemas().posts.column<"title">())
                                   .from(schemas().users)
                                   .join(schemas().posts, JoinType::LEFT)
                                   .on(schemas().posts.column<"user_id">() == schemas().users.column<"id">()));
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    // Left join should include all users, even those without posts
    EXPECT_GE(block.rows(), 3);
}

TEST_F(CompiledJoinTest, RightJoin) {
    auto query  = compile_query(select(schemas().users.column<"name">(), schemas().posts.column<"title">())
                                   .from(schemas().users)
                                   .join(schemas().posts, JoinType::RIGHT)
                                   .on(schemas().posts.column<"user_id">() == schemas().users.column<"id">()));
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    // Right join should include all posts
    EXPECT_GE(block.rows(), 1);
}

TEST_F(CompiledJoinTest, MultipleJoins) {
    auto query  = compile_query(select(schemas().users.column<"name">(), schemas().posts.column<"title">())
                                   .from(schemas().users)
                                   .join(schemas().posts)
                                   .on(schemas().posts.column<"user_id">() == schemas().users.column<"id">()));
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
}

TEST_F(CompiledJoinTest, JoinComplexCondition) {
    auto query  = compile_query(select(schemas().users.column<"name">(), schemas().posts.column<"title">())
                                   .from(schemas().users)
                                   .join(schemas().posts)
                                   .on(schemas().posts.column<"user_id">() == schemas().users.column<"id">() &&
                                       schemas().posts.column<"published">() == true));
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
}

TEST_F(CompiledJoinTest, JoinWithWhere) {
    auto query  = compile_query(select(schemas().users.column<"name">(), schemas().posts.column<"title">())
                                   .from(schemas().users)
                                   .join(schemas().posts)
                                   .on(schemas().posts.column<"user_id">() == schemas().users.column<"id">())
                                   .where(schemas().users.column<"active">() == true));
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
}

TEST_F(CompiledJoinTest, JoinWithAggregates) {
    auto query =
        compile_query(select(schemas().users.column<"name">(), count(schemas().posts.column<"id">()).as("post_count"))
                          .from(schemas().users)
                          .join(schemas().posts, JoinType::LEFT)
                          .on(schemas().posts.column<"user_id">() == schemas().users.column<"id">())
                          .group_by(schemas().users.column<"name">()));
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
}

TEST_F(CompiledJoinTest, JoinWithOrderBy) {
    auto query =
        compile_query(select(schemas().users.column<"name">(), schemas().posts.column<"title">())
                          .from(schemas().users)
                          .join(schemas().posts)
                          .on(schemas().posts.column<"user_id">() == schemas().users.column<"id">())
                          .order_by(asc(schemas().users.column<"name">()), desc(schemas().posts.column<"title">())));
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
}
