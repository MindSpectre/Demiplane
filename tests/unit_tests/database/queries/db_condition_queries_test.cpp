// Condition query expression tests
// Comprehensive tests for condition expressions and operators

#include <gtest/gtest.h>

#include "query_expressions.hpp"
#include "db_column.hpp"
#include "db_field_schema.hpp"
#include "db_table_schema.hpp"
#include "postgres_dialect.hpp"
#include "query_compiler.hpp"

using namespace demiplane::db;

#define MANUAL_CHECK

// Test fixture for condition operations
class ConditionQueryTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create test schemas
        users_schema = std::make_shared<TableSchema>("users");
        users_schema->add_field<int>("id", "INTEGER")
                    .primary_key("id")
                    .add_field<std::string>("name", "VARCHAR(255)")
                    .add_field<int>("age", "INTEGER")
                    .add_field<bool>("active", "BOOLEAN");

        posts_schema = std::make_shared<TableSchema>("posts");
        posts_schema->add_field<int>("id", "INTEGER")
                    .primary_key("id")
                    .add_field<int>("user_id", "INTEGER")
                    .add_field<std::string>("title", "VARCHAR(255)")
                    .add_field<bool>("published", "BOOLEAN");

        // Create column references
        user_id = users_schema->column<int>("id");
        user_name = users_schema->column<std::string>("name");
        user_age = users_schema->column<int>("age");
        user_active = users_schema->column<bool>("active");

        post_id = posts_schema->column<int>("id");
        post_user_id = posts_schema->column<int>("user_id");
        post_title = posts_schema->column<std::string>("title");
        post_published = posts_schema->column<bool>("published");

        // Create compiler
        compiler = std::make_unique<QueryCompiler>(std::make_unique<PostgresDialect>(), false);
    }

    std::shared_ptr<TableSchema> users_schema;
    std::shared_ptr<TableSchema> posts_schema;
    
    Column<int> user_id{nullptr, ""};
    Column<std::string> user_name{nullptr, ""};
    Column<int> user_age{nullptr, ""};
    Column<bool> user_active{nullptr, ""};
    
    Column<int> post_id{nullptr, ""};
    Column<int> post_user_id{nullptr, ""};
    Column<std::string> post_title{nullptr, ""};
    Column<bool> post_published{nullptr, ""};
    
    std::unique_ptr<QueryCompiler> compiler;
};

// Test binary condition expressions
TEST_F(ConditionQueryTest, BinaryConditionExpressions) {
    // Equality
    auto eq_query = select(user_name).from(users_schema).where(user_age == lit(25));
    auto eq_result = compiler->compile(eq_query);
    EXPECT_FALSE(eq_result.sql.empty());
    
    // Inequality
    auto neq_query = select(user_name).from(users_schema).where(user_age != lit(25));
    auto neq_result = compiler->compile(neq_query);
    EXPECT_FALSE(neq_result.sql.empty());
    
    // Greater than
    auto gt_query = select(user_name).from(users_schema).where(user_age > lit(18));
    auto gt_result = compiler->compile(gt_query);
    EXPECT_FALSE(gt_result.sql.empty());
    
    // Greater than or equal
    auto gte_query = select(user_name).from(users_schema).where(user_age >= lit(18));
    auto gte_result = compiler->compile(gte_query);
    EXPECT_FALSE(gte_result.sql.empty());
    
    // Less than
    auto lt_query = select(user_name).from(users_schema).where(user_age < lit(65));
    auto lt_result = compiler->compile(lt_query);
    EXPECT_FALSE(lt_result.sql.empty());
    
    // Less than or equal
    auto lte_query = select(user_name).from(users_schema).where(user_age <= lit(65));
    auto lte_result = compiler->compile(lte_query);
    EXPECT_FALSE(lte_result.sql.empty());
    
    #ifdef MANUAL_CHECK
        std::cout << "EQ: " << eq_result.sql << std::endl;
        std::cout << "NEQ: " << neq_result.sql << std::endl;
        std::cout << "GT: " << gt_result.sql << std::endl;
        std::cout << "GTE: " << gte_result.sql << std::endl;
        std::cout << "LT: " << lt_result.sql << std::endl;
        std::cout << "LTE: " << lte_result.sql << std::endl;
    #endif
}

// Test logical condition expressions
TEST_F(ConditionQueryTest, LogicalConditionExpressions) {
    // AND
    auto and_query = select(user_name).from(users_schema)
                     .where(user_age > lit(18) && user_active == lit(true));
    auto and_result = compiler->compile(and_query);
    EXPECT_FALSE(and_result.sql.empty());
    
    // OR
    auto or_query = select(user_name).from(users_schema)
                    .where(user_age < lit(18) || user_age > lit(65));
    auto or_result = compiler->compile(or_query);
    EXPECT_FALSE(or_result.sql.empty());
    
    #ifdef MANUAL_CHECK
        std::cout << "AND: " << and_result.sql << std::endl;
        std::cout << "OR: " << or_result.sql << std::endl;
    #endif
}

// Test unary condition expressions
TEST_F(ConditionQueryTest, UnaryConditionExpressions) {
    // NOT (using boolean column)
    auto not_query = select(user_name).from(users_schema).where(user_active == lit(false));
    auto not_result = compiler->compile(not_query);
    EXPECT_FALSE(not_result.sql.empty());
    
    #ifdef MANUAL_CHECK
        std::cout << "NOT condition: " << not_result.sql << std::endl;
    #endif
}

// Test string comparison expressions (placeholder for LIKE when available)
TEST_F(ConditionQueryTest, StringComparisonExpressions) {
    // String equality comparison
    auto str_eq_query = select(user_name).from(users_schema).where(user_name == lit("john"));
    auto str_eq_result = compiler->compile(str_eq_query);
    EXPECT_FALSE(str_eq_result.sql.empty());
    
    #ifdef MANUAL_CHECK
        std::cout << "String equality: " << str_eq_result.sql << std::endl;
    #endif
}

// Test BETWEEN expressions
TEST_F(ConditionQueryTest, BetweenExpressions) {
    auto query = select(user_name)
                 .from(users_schema)
                 .where(between(user_age, lit(18), lit(65)));
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());
    #ifdef MANUAL_CHECK
        std::cout << result.sql << std::endl;
    #endif
}

// Test IN LIST expressions
TEST_F(ConditionQueryTest, InListExpressions) {
    auto query = select(user_name)
                 .from(users_schema)
                 .where(in(user_age, lit(18), lit(25), lit(30)));
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());
    #ifdef MANUAL_CHECK
        std::cout << result.sql << std::endl;
    #endif
}

// Test EXISTS expressions
TEST_F(ConditionQueryTest, ExistsExpressions) {
    auto subquery = select(lit(1))
                    .from(posts_schema)
                    .where(post_user_id == user_id && post_published == lit(true));
    
    auto query = select(user_name)
                 .from(users_schema)
                 .where(exists(subquery));
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());
    #ifdef MANUAL_CHECK
        std::cout << result.sql << std::endl;
    #endif
}

// Test SUBQUERY conditions
TEST_F(ConditionQueryTest, SubqueryConditions) {
    auto active_users = select(user_id)
                        .from(users_schema)
                        .where(user_active == lit(true));
    
    auto query = select(post_title)
                 .from(posts_schema)
                 .where(in(post_user_id, subquery(active_users)));
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());
    #ifdef MANUAL_CHECK
        std::cout << result.sql << std::endl;
    #endif
}

// Test complex nested conditions
TEST_F(ConditionQueryTest, ComplexNestedConditions) {
    auto query = select(user_name)
                 .from(users_schema)
                 .where((user_age > lit(18) && user_age < lit(65)) || 
                        (user_active == lit(true) && user_age >= lit(65)));
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());
    #ifdef MANUAL_CHECK
        std::cout << result.sql << std::endl;
    #endif
}