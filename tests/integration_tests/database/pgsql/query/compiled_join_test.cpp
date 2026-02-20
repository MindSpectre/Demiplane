// Compiled JOIN Query Functional Tests
// Tests query compilation + execution with SyncExecutor using QueryLibrary

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
    auto query  = library().produce<join::InnerJoin>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_GE(block.rows(), 1);
}

TEST_F(CompiledJoinTest, LeftJoin) {
    auto query  = library().produce<join::LeftJoin>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    // Left join should include all users, even those without posts
    EXPECT_GE(block.rows(), 3);
}

TEST_F(CompiledJoinTest, RightJoin) {
    auto query  = library().produce<join::RightJoin>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    // Right join should include all posts
    EXPECT_GE(block.rows(), 1);
}

TEST_F(CompiledJoinTest, MultipleJoins) {
    auto query  = library().produce<join::MultipleJoins>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
}

TEST_F(CompiledJoinTest, JoinComplexCondition) {
    auto query  = library().produce<join::JoinComplexCondition>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
}

TEST_F(CompiledJoinTest, JoinWithWhere) {
    auto query  = library().produce<join::JoinWithWhere>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
}

TEST_F(CompiledJoinTest, JoinWithAggregates) {
    auto query  = library().produce<join::JoinWithAggregates>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
}

TEST_F(CompiledJoinTest, JoinWithOrderBy) {
    auto query  = library().produce<join::JoinWithOrderBy>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
}
