// Compiled SUBQUERY Query Functional Tests
// Tests query compilation + execution with SyncExecutor using QueryLibrary

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
    auto query  = library().produce<subq::SubqueryInWhere>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    // Should return posts by active users
    EXPECT_GE(block.rows(), 1);
}

// ============== EXISTS Tests ==============

TEST_F(CompiledSubqueryTest, Exists) {
    auto query  = library().produce<subq::Exists>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    // Should return users who have published posts
    EXPECT_GE(block.rows(), 1);
}

TEST_F(CompiledSubqueryTest, NotExists) {
    auto query  = library().produce<subq::NotExists>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
}

// ============== IN Subquery Tests ==============

TEST_F(CompiledSubqueryTest, InSubqueryMultiple) {
    auto query  = library().produce<subq::InSubqueryMultiple>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
}

// ============== Nested Subquery Tests ==============

TEST_F(CompiledSubqueryTest, NestedSubqueries) {
    auto query  = library().produce<subq::NestedSubqueries>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
}

// ============== Subquery with Aggregates ==============

TEST_F(CompiledSubqueryTest, SubqueryWithAggregates) {
    auto query  = library().produce<subq::SubqueryWithAggregates>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
}

// ============== Subquery with DISTINCT ==============

TEST_F(CompiledSubqueryTest, SubqueryWithDistinct) {
    auto query  = library().produce<subq::SubqueryWithDistinct>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
}
