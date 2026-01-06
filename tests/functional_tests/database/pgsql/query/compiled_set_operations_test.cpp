// Compiled SET Operations Query Functional Tests
// Tests query compilation + execution with SyncExecutor using QueryLibrary

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
    auto query  = library().produce<set_op::UnionBasic>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    // UNION removes duplicates
    EXPECT_GE(block.rows(), 1);
}

TEST_F(CompiledSetOperationsTest, UnionAll) {
    auto query  = library().produce<set_op::UnionAll>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    // UNION ALL keeps all rows including duplicates
    EXPECT_GE(block.rows(), 1);
}

// ============== INTERSECT Tests ==============

TEST_F(CompiledSetOperationsTest, Intersect) {
    auto query  = library().produce<set_op::Intersect>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    // Returns only rows that appear in both result sets
}

// ============== EXCEPT Tests ==============

TEST_F(CompiledSetOperationsTest, Except) {
    auto query  = library().produce<set_op::Except>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    // Returns rows in first set but not in second
}

// ============== Combined SET Operations ==============

TEST_F(CompiledSetOperationsTest, UnionWithOrderBy) {
    auto query  = library().produce<set_op::UnionWithOrderBy>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_GE(block.rows(), 1);
}

TEST_F(CompiledSetOperationsTest, UnionWithLimit) {
    auto query  = library().produce<set_op::UnionWithLimit>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_LE(block.rows(), 10);  // Limited to 10 rows
}

TEST_F(CompiledSetOperationsTest, MultipleUnions) {
    auto query  = library().produce<set_op::MultipleUnions>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
}

TEST_F(CompiledSetOperationsTest, MixedSetOps) {
    auto query  = library().produce<set_op::MixedSetOps>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
}
