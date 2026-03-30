// Compiled CONDITION Query Functional Tests
// Tests query compilation + execution with SyncExecutor

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
    auto query = compile_query(
        select(schemas().users.column<"name">()).from(schemas().users).where(schemas().users.column<"age">() == 25));
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_EQ(block.rows(), 1);  // Only john has age == 25
}

TEST_F(CompiledConditionTest, BinaryNotEqual) {
    auto query = compile_query(
        select(schemas().users.column<"name">()).from(schemas().users).where(schemas().users.column<"age">() != 25));
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_EQ(block.rows(), 4);  // Everyone except age == 25
}

TEST_F(CompiledConditionTest, BinaryGreater) {
    auto query = compile_query(
        select(schemas().users.column<"name">()).from(schemas().users).where(schemas().users.column<"age">() > 18));
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_GE(block.rows(), 1);  // Users with age > 18
}

TEST_F(CompiledConditionTest, BinaryGreaterEqual) {
    auto query = compile_query(
        select(schemas().users.column<"name">()).from(schemas().users).where(schemas().users.column<"age">() >= 18));
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_GE(block.rows(), 1);  // Users with age >= 18
}

TEST_F(CompiledConditionTest, BinaryLess) {
    auto query = compile_query(
        select(schemas().users.column<"name">()).from(schemas().users).where(schemas().users.column<"age">() < 65));
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_GE(block.rows(), 1);  // Users with age < 65
}

TEST_F(CompiledConditionTest, BinaryLessEqual) {
    auto query = compile_query(
        select(schemas().users.column<"name">()).from(schemas().users).where(schemas().users.column<"age">() <= 65));
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_GE(block.rows(), 1);  // Users with age <= 65
}

// ============== Logical Operator Tests ==============

TEST_F(CompiledConditionTest, LogicalAnd) {
    auto query =
        compile_query(select(schemas().users.column<"name">())
                          .from(schemas().users)
                          .where(schemas().users.column<"age">() > 18 && schemas().users.column<"active">() == true));
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    // Users with age > 18 AND active == true
    EXPECT_GE(block.rows(), 1);
}

TEST_F(CompiledConditionTest, LogicalOr) {
    auto query =
        compile_query(select(schemas().users.column<"name">())
                          .from(schemas().users)
                          .where(schemas().users.column<"age">() < 18 || schemas().users.column<"age">() > 65));
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    // Users with age < 18 OR age > 65
    EXPECT_GE(block.rows(), 1);
}

TEST_F(CompiledConditionTest, UnaryCondition) {
    auto query  = compile_query(select(schemas().users.column<"name">())
                                   .from(schemas().users)
                                   .where(schemas().users.column<"active">() == false));
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    // Users with active == false
    EXPECT_EQ(block.rows(), 2);  // bob and charlie
}

// ============== String Comparison Tests ==============

TEST_F(CompiledConditionTest, StringComparison) {
    auto query  = compile_query(select(schemas().users.column<"name">())
                                   .from(schemas().users)
                                   .where(schemas().users.column<"name">() == "john"));
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    EXPECT_EQ(block.rows(), 1);  // Only john
}

// ============== Range Tests ==============

TEST_F(CompiledConditionTest, Between) {
    auto query  = compile_query(select(schemas().users.column<"name">())
                                   .from(schemas().users)
                                   .where(between(schemas().users.column<"age">(), 18, 65)));
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    // Users with age BETWEEN 18 AND 65
    EXPECT_GE(block.rows(), 1);
}

TEST_F(CompiledConditionTest, InList) {
    auto query  = compile_query(select(schemas().users.column<"name">())
                                   .from(schemas().users)
                                   .where(in(schemas().users.column<"age">(), 18, 25, 30)));
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    auto& block = result.value();
    // Users with age IN (18, 25, 30)
    EXPECT_GE(block.rows(), 1);
}

// ============== Exists Tests ==============

TEST_F(CompiledConditionTest, ExistsCondition) {
    const auto& s = schemas();
    auto subq     = select(lit(1)).from(s.posts).where(s.posts.column<"user_id">() == s.users.column<"id">() &&
                                                   s.posts.column<"published">() == lit(true));
    auto query    = compile_query(select(s.users.column<"name">()).from(s.users).where(exists(subq)));
    auto result   = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
}

TEST_F(CompiledConditionTest, SubqueryCondition) {
    const auto& s     = schemas();
    auto active_users = select(s.users.column<"id">()).from(s.users).where(s.users.column<"active">() == true);
    auto query        = compile_query(
        select(s.posts.column<"title">()).from(s.posts).where(in(s.posts.column<"user_id">(), subquery(active_users))));
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
}

// ============== Complex Nested Tests ==============

TEST_F(CompiledConditionTest, ComplexNested) {
    auto query = compile_query(
        select(schemas().users.column<"name">())
            .from(schemas().users)
            .where((schemas().users.column<"age">() > 18 && schemas().users.column<"age">() < 65) ||
                   (schemas().users.column<"active">() == true && schemas().users.column<"age">() >= 65)));
    auto result = executor().execute(query);

    ASSERT_TRUE(result.is_success()) << "Query failed: " << result.error<ErrorContext>();
    // Complex nested: (age > 18 && age < 65) || (active == true && age >= 65)
}
