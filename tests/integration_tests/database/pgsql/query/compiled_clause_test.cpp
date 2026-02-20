// Compiled CLAUSE Query Functional Tests
// Tests query compilation + execution with SyncExecutor using QueryLibrary

#include <test_fixture.hpp>

using namespace demiplane::db;
using namespace demiplane::db::postgres;
using namespace demiplane::test;

class CompiledClauseTest : public PgsqlTestFixture {
protected:
    void SetUp() override {
        PgsqlTestFixture::SetUp();
        if (connection() == nullptr)
            return;
        CreateClauseTestTables();
        InsertClauseTestData();
    }

    void TearDown() override {
        if (connection()) {
            DropClauseTestTables();
        }
        PgsqlTestFixture::TearDown();
    }
};

// ============== FROM Clause Tests ==============

TEST_F(CompiledClauseTest, FromTable) {
    auto query  = library().produce<clause::FromTable>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_GE(block.rows(), 1);
}

TEST_F(CompiledClauseTest, FromTableName) {
    auto query  = library().produce<clause::FromTableName>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
}

// ============== WHERE Clause Tests ==============

TEST_F(CompiledClauseTest, WhereSimple) {
    auto query  = library().produce<clause::WhereSimple>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    // Should return only active users
    EXPECT_GE(block.rows(), 1);
}

TEST_F(CompiledClauseTest, WhereComplex) {
    auto query  = library().produce<clause::WhereComplex>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
}

TEST_F(CompiledClauseTest, WhereIn) {
    auto query  = library().produce<clause::WhereIn>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
}

TEST_F(CompiledClauseTest, WhereBetween) {
    auto query  = library().produce<clause::WhereBetween>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
}

// ============== GROUP BY Clause Tests ==============

TEST_F(CompiledClauseTest, GroupBySingle) {
    auto query  = library().produce<clause::GroupBySingle>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    // Should have groups for each department
    EXPECT_GE(block.rows(), 1);
}

TEST_F(CompiledClauseTest, GroupByMultiple) {
    auto query  = library().produce<clause::GroupByMultiple>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
}

TEST_F(CompiledClauseTest, GroupByWithWhere) {
    auto query  = library().produce<clause::GroupByWithWhere>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
}

// ============== HAVING Clause Tests ==============

TEST_F(CompiledClauseTest, HavingSimple) {
    auto query  = library().produce<clause::HavingSimple>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
}

TEST_F(CompiledClauseTest, HavingMultiple) {
    auto query  = library().produce<clause::HavingMultiple>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
}

TEST_F(CompiledClauseTest, HavingWithWhere) {
    auto query  = library().produce<clause::HavingWithWhere>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
}

// ============== ORDER BY Clause Tests ==============

TEST_F(CompiledClauseTest, OrderByAsc) {
    auto query  = library().produce<clause::OrderByAsc>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_GE(block.rows(), 1);
}

TEST_F(CompiledClauseTest, OrderByDesc) {
    auto query  = library().produce<clause::OrderByDesc>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_GE(block.rows(), 1);
}

TEST_F(CompiledClauseTest, OrderByMultiple) {
    auto query  = library().produce<clause::OrderByMultiple>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
}

// ============== LIMIT Clause Tests ==============

TEST_F(CompiledClauseTest, LimitBasic) {
    auto query  = library().produce<clause::LimitBasic>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_LE(block.rows(), 10);
}

TEST_F(CompiledClauseTest, LimitWithOrderBy) {
    auto query  = library().produce<clause::LimitWithOrderBy>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_LE(block.rows(), 5);
}

TEST_F(CompiledClauseTest, LimitWithWhereOrderBy) {
    auto query  = library().produce<clause::LimitWithWhereOrderBy>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_LE(block.rows(), 20);
}

// ============== Complex Combined Clause Tests ==============

TEST_F(CompiledClauseTest, ComplexAllClauses) {
    auto query  = library().produce<clause::ComplexAllClauses>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
}

TEST_F(CompiledClauseTest, ClausesWithJoins) {
    auto query  = library().produce<clause::ClausesWithJoins>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
}
