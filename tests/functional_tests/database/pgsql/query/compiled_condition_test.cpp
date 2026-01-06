// Compiled CONDITION Query Functional Tests
// Tests query compilation + execution with SyncExecutor using QueryLibrary

#include <test_fixture.hpp>

using namespace demiplane::db;
using namespace demiplane::db::postgres;
using namespace demiplane::test;

class CompiledConditionTest : public PgsqlTestFixture {
protected:
    void SetUp() override {
        PgsqlTestFixture::SetUp();
        if (connection() == nullptr)
            return;
        CreateStandardTables();
        InsertConditionTestData();
    }

    void TearDown() override {
        if (connection()) {
            DropStandardTables();
        }
        PgsqlTestFixture::TearDown();
    }
};

// ============== Binary Comparison Tests ==============

TEST_F(CompiledConditionTest, BinaryEqual) {
    auto query  = library().produce<condition::BinaryEqual>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_EQ(block.rows(), 1);  // Only john has age == 25
}

TEST_F(CompiledConditionTest, BinaryNotEqual) {
    auto query  = library().produce<condition::BinaryNotEqual>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_EQ(block.rows(), 4);  // Everyone except age == 25
}

TEST_F(CompiledConditionTest, BinaryGreater) {
    auto query  = library().produce<condition::BinaryGreater>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_GE(block.rows(), 1);  // Users with age > 18
}

TEST_F(CompiledConditionTest, BinaryGreaterEqual) {
    auto query  = library().produce<condition::BinaryGreaterEqual>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_GE(block.rows(), 1);  // Users with age >= 18
}

TEST_F(CompiledConditionTest, BinaryLess) {
    auto query  = library().produce<condition::BinaryLess>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_GE(block.rows(), 1);  // Users with age < 65
}

TEST_F(CompiledConditionTest, BinaryLessEqual) {
    auto query  = library().produce<condition::BinaryLessEqual>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_GE(block.rows(), 1);  // Users with age <= 65
}

// ============== Logical Operator Tests ==============

TEST_F(CompiledConditionTest, LogicalAnd) {
    auto query  = library().produce<condition::LogicalAnd>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    // Users with age > 18 AND active == true
    EXPECT_GE(block.rows(), 1);
}

TEST_F(CompiledConditionTest, LogicalOr) {
    auto query  = library().produce<condition::LogicalOr>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    // Users with age < 18 OR age > 65
    EXPECT_GE(block.rows(), 1);
}

TEST_F(CompiledConditionTest, UnaryCondition) {
    auto query  = library().produce<condition::UnaryCondition>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    // Users with active == false
    EXPECT_EQ(block.rows(), 2);  // bob and charlie
}

// ============== String Comparison Tests ==============

TEST_F(CompiledConditionTest, StringComparison) {
    auto query  = library().produce<condition::StringComparison>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_EQ(block.rows(), 1);  // Only john
}

// ============== Range Tests ==============

TEST_F(CompiledConditionTest, Between) {
    auto query  = library().produce<condition::Between>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    // Users with age BETWEEN 18 AND 65
    EXPECT_GE(block.rows(), 1);
}

TEST_F(CompiledConditionTest, InList) {
    auto query  = library().produce<condition::InList>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    // Users with age IN (18, 25, 30)
    EXPECT_GE(block.rows(), 1);
}

// ============== Exists Tests ==============

TEST_F(CompiledConditionTest, ExistsCondition) {
    auto query  = library().produce<condition::ExistsCondition>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
}

TEST_F(CompiledConditionTest, SubqueryCondition) {
    auto query  = library().produce<condition::SubqueryCondition>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
}

// ============== Complex Nested Tests ==============

TEST_F(CompiledConditionTest, ComplexNested) {
    auto query  = library().produce<condition::ComplexNested>();
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    // Complex nested: (age > 18 && age < 65) || (active == true && age >= 65)
}
