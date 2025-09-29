// Condition query expression tests
// Comprehensive tests for condition expressions and operators

#include <demiplane/scroll>

#include <postgres_dialect.hpp>
#include <query_compiler.hpp>

#include "common.hpp"

#include <gtest/gtest.h>

using namespace demiplane::db;


// Test fixture for condition operations
class ConditionQueryTest : public ::testing::Test, public demiplane::scroll::LoggerProvider {
protected:
    void SetUp() override {
        SET_COMMON_LOGGER();
        // Create test schemas
        users_schema = std::make_shared<Table>("users");
        users_schema->add_field<int>("id", "INTEGER")
            .primary_key("id")
            .add_field<std::string>("name", "VARCHAR(255)")
            .add_field<int>("age", "INTEGER")
            .add_field<bool>("active", "BOOLEAN");

        posts_schema = std::make_shared<Table>("posts");
        posts_schema->add_field<int>("id", "INTEGER")
            .primary_key("id")
            .add_field<int>("user_id", "INTEGER")
            .add_field<std::string>("title", "VARCHAR(255)")
            .add_field<bool>("published", "BOOLEAN");

        // Create column references
        user_id     = users_schema->column<int>("id");
        user_name   = users_schema->column<std::string>("name");
        user_age    = users_schema->column<int>("age");
        user_active = users_schema->column<bool>("active");

        post_id        = posts_schema->column<int>("id");
        post_user_id   = posts_schema->column<int>("user_id");
        post_title     = posts_schema->column<std::string>("title");
        post_published = posts_schema->column<bool>("published");

        // Create compiler
        compiler = std::make_unique<QueryCompiler>(std::make_unique<postgres::Dialect>(), false);
    }

    std::shared_ptr<Table> users_schema;
    std::shared_ptr<Table> posts_schema;

    TableColumn<int> user_id{nullptr, ""};
    TableColumn<std::string> user_name{nullptr, ""};
    TableColumn<int> user_age{nullptr, ""};
    TableColumn<bool> user_active{nullptr, ""};

    TableColumn<int> post_id{nullptr, ""};
    TableColumn<int> post_user_id{nullptr, ""};
    TableColumn<std::string> post_title{nullptr, ""};
    TableColumn<bool> post_published{nullptr, ""};

    std::unique_ptr<QueryCompiler> compiler;
};

// Test binary condition expressions
TEST_F(ConditionQueryTest, BinaryConditionExpressions) {
    // Equality
    auto eq_query  = select(user_name).from(users_schema).where(user_age == 25);
    auto eq_result = compiler->compile(eq_query);
    EXPECT_FALSE(eq_result.sql().empty());

    // Inequality
    auto neq_query  = select(user_name).from(users_schema).where(user_age != 25);
    auto neq_result = compiler->compile(neq_query);
    EXPECT_FALSE(neq_result.sql().empty());

    // Greater than
    auto gt_query  = select(user_name).from(users_schema).where(user_age > 18);
    auto gt_result = compiler->compile(gt_query);
    EXPECT_FALSE(gt_result.sql().empty());

    // Greater than or equal
    auto gte_query  = select(user_name).from(users_schema).where(user_age >= 18);
    auto gte_result = compiler->compile(gte_query);
    EXPECT_FALSE(gte_result.sql().empty());

    // Less than
    auto lt_query  = select(user_name).from(users_schema).where(user_age < 65);
    auto lt_result = compiler->compile(lt_query);
    EXPECT_FALSE(lt_result.sql().empty());

    // Less than or equal
    auto lte_query  = select(user_name).from(users_schema).where(user_age <= 65);
    auto lte_result = compiler->compile(lte_query);
    EXPECT_FALSE(lte_result.sql().empty());

    SCROLL_LOG_INF() << "EQ: " << eq_result.sql();
    SCROLL_LOG_INF() << "NEQ: " << neq_result.sql();
    SCROLL_LOG_INF() << "GT: " << gt_result.sql();
    SCROLL_LOG_INF() << "GTE: " << gte_result.sql();
    SCROLL_LOG_INF() << "LT: " << lt_result.sql();
    SCROLL_LOG_INF() << "LTE: " << lte_result.sql();
}

// Test logical condition expressions
TEST_F(ConditionQueryTest, LogicalConditionExpressions) {
    // AND
    auto and_query  = select(user_name).from(users_schema).where(user_age > 18 && user_active == true);
    auto and_result = compiler->compile(and_query);
    EXPECT_FALSE(and_result.sql().empty());

    // OR
    auto or_query  = select(user_name).from(users_schema).where(user_age < 18 || user_age > 65);
    auto or_result = compiler->compile(or_query);
    EXPECT_FALSE(or_result.sql().empty());

    SCROLL_LOG_INF() << "AND: " << and_result.sql();
    SCROLL_LOG_INF() << "OR: " << or_result.sql();
}

// Test unary condition expressions
TEST_F(ConditionQueryTest, UnaryConditionExpressions) {
    // NOT (using boolean column)
    auto not_query  = select(user_name).from(users_schema).where(user_active == false);
    auto not_result = compiler->compile(not_query);
    EXPECT_FALSE(not_result.sql().empty());

    SCROLL_LOG_INF() << "NOT condition: " << not_result.sql();
}

// Test string comparison expressions (placeholder for LIKE when available)
TEST_F(ConditionQueryTest, StringComparisonExpressions) {
    // String equality comparison
    auto str_eq_query  = select(user_name).from(users_schema).where(user_name == "john");
    auto str_eq_result = compiler->compile(str_eq_query);
    EXPECT_FALSE(str_eq_result.sql().empty());

    SCROLL_LOG_INF() << "String equality: " << str_eq_result.sql();
}

// Test BETWEEN expressions
TEST_F(ConditionQueryTest, BetweenExpressions) {
    auto query  = select(user_name).from(users_schema).where(between(user_age, 18, 65));
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql().empty());
    SCROLL_LOG_INF() << result.sql();
}

// Test IN LIST expressions
TEST_F(ConditionQueryTest, InListExpressions) {
    auto query  = select(user_name).from(users_schema).where(in(user_age, 18, 25, 30));
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql().empty());
    SCROLL_LOG_INF() << result.sql();
}

// Test EXISTS expressions
TEST_F(ConditionQueryTest, ExistsExpressions) {
    auto subquery = select(lit(1)).from(posts_schema).where(post_user_id == user_id && post_published == lit(true));

    auto query  = select(user_name).from(users_schema).where(exists(subquery));
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql().empty());
    SCROLL_LOG_INF() << result.sql();
}

// Test SUBQUERY conditions
TEST_F(ConditionQueryTest, SubqueryConditions) {
    auto active_users = select(user_id).from(users_schema).where(user_active == true);

    auto query  = select(post_title).from(posts_schema).where(in(post_user_id, subquery(active_users)));
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql().empty());
    SCROLL_LOG_INF() << result.sql();
}

// Test complex nested conditions
TEST_F(ConditionQueryTest, ComplexNestedConditions) {
    auto query = select(user_name)
                     .from(users_schema)
                     .where((user_age > 18 && user_age < 65) || (user_active == true && user_age >= 65));
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql().empty());
    SCROLL_LOG_INF() << result.sql();
}
