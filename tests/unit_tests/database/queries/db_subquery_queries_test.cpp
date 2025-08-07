// Subquery and EXISTS expression tests
// Comprehensive tests for subquery operations

#include <gtest/gtest.h>

#include "query_expressions.hpp"
#include "db_column.hpp"
#include "db_field_schema.hpp"
#include "db_table_schema.hpp"
#include "postgres_dialect.hpp"
#include "query_compiler.hpp"

using namespace demiplane::db;

#define MANUAL_CHECK

// Test fixture for subquery operations
class SubqueryTest : public ::testing::Test {
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

        orders_schema = std::make_shared<TableSchema>("orders");
        orders_schema->add_field<int>("id", "INTEGER")
                     .primary_key("id")
                     .add_field<int>("user_id", "INTEGER")
                     .add_field<double>("amount", "DECIMAL(10,2)")
                     .add_field<bool>("completed", "BOOLEAN");

        // Create column references
        user_id = users_schema->column<int>("id");
        user_name = users_schema->column<std::string>("name");
        user_age = users_schema->column<int>("age");
        user_active = users_schema->column<bool>("active");

        post_id = posts_schema->column<int>("id");
        post_user_id = posts_schema->column<int>("user_id");
        post_title = posts_schema->column<std::string>("title");
        post_published = posts_schema->column<bool>("published");

        order_id = orders_schema->column<int>("id");
        order_user_id = orders_schema->column<int>("user_id");
        order_amount = orders_schema->column<double>("amount");
        order_completed = orders_schema->column<bool>("completed");

        // Create compiler
        compiler = std::make_unique<QueryCompiler>(std::make_unique<PostgresDialect>(), false);
    }

    std::shared_ptr<TableSchema> users_schema;
    std::shared_ptr<TableSchema> posts_schema;
    std::shared_ptr<TableSchema> orders_schema;
    
    Column<int> user_id{nullptr, ""};
    Column<std::string> user_name{nullptr, ""};
    Column<int> user_age{nullptr, ""};
    Column<bool> user_active{nullptr, ""};
    
    Column<int> post_id{nullptr, ""};
    Column<int> post_user_id{nullptr, ""};
    Column<std::string> post_title{nullptr, ""};
    Column<bool> post_published{nullptr, ""};
    
    Column<int> order_id{nullptr, ""};
    Column<int> order_user_id{nullptr, ""};
    Column<double> order_amount{nullptr, ""};
    Column<bool> order_completed{nullptr, ""};
    
    std::unique_ptr<QueryCompiler> compiler;
};

// Test basic subquery in WHERE clause
TEST_F(SubqueryTest, SubqueryInWhereExpression) {
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

// Test EXISTS expression
TEST_F(SubqueryTest, ExistsExpression) {
    auto published_posts_subquery = select(lit(1))
                                   .from(posts_schema)
                                   .where(post_user_id == user_id && post_published == lit(true));
    
    auto query = select(user_name)
                 .from(users_schema)
                 .where(exists(published_posts_subquery));
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());
    #ifdef MANUAL_CHECK
        std::cout << result.sql << std::endl;
    #endif
}

// Test NOT EXISTS expression
TEST_F(SubqueryTest, NotExistsExpression) {
    auto pending_orders_subquery = select(lit(1))
                                  .from(orders_schema)
                                  .where(order_user_id == user_id && order_completed == lit(false));
    
    auto query = select(user_name)
                 .from(users_schema)
                 .where(!exists(pending_orders_subquery));
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());
    #ifdef MANUAL_CHECK
        std::cout << result.sql << std::endl;
    #endif
}

// Test basic subquery compilation
TEST_F(SubqueryTest, BasicSubqueryCompilationExpression) {
    auto post_count_subquery = select(count(post_id))
                               .from(posts_schema)
                               .where(post_user_id == user_id);
    
    auto query = subquery(post_count_subquery);
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());
    #ifdef MANUAL_CHECK
        std::cout << result.sql << std::endl;
    #endif
}

// Test subquery structure
TEST_F(SubqueryTest, SubqueryStructureExpression) {
    auto user_post_count = select(count(post_id))
                           .from(posts_schema)
                           .where(post_user_id == user_id);
    
    auto sub = subquery(user_post_count);
    auto result = compiler->compile(sub);
    EXPECT_FALSE(result.sql.empty());
    #ifdef MANUAL_CHECK
        std::cout << result.sql << std::endl;
    #endif
}

// Test IN with multiple values subquery
TEST_F(SubqueryTest, InSubqueryMultipleValuesExpression) {
    auto high_value_users = select(user_id)
                            .from(orders_schema)
                            .where(order_amount > lit(1000.0))
                            .group_by(order_user_id)
                            .having(sum(order_amount) > lit(5000.0));
    
    auto query = select(user_name)
                 .from(users_schema)
                 .where(in(user_id, subquery(high_value_users)));
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());
    #ifdef MANUAL_CHECK
        std::cout << result.sql << std::endl;
    #endif
}

// Test nested subqueries
TEST_F(SubqueryTest, NestedSubqueriesExpression) {
    auto users_with_completed_orders = select(order_user_id)
                                       .from(orders_schema)
                                       .where(order_completed == lit(true));
    
    auto posts_by_active_users = select(post_user_id)
                                 .from(posts_schema)
                                 .where(in(post_user_id, subquery(users_with_completed_orders)));
    
    auto query = select(user_name)
                 .from(users_schema)
                 .where(in(user_id, subquery(posts_by_active_users)));
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());
    #ifdef MANUAL_CHECK
        std::cout << result.sql << std::endl;
    #endif
}

// Test subquery with aggregates
TEST_F(SubqueryTest, SubqueryWithAggregatesExpression) {
    auto avg_order_amount = select(avg(order_amount))
                            .from(orders_schema)
                            .where(order_completed == lit(true));
    
    auto query = select(user_name, order_amount)
                 .from(users_schema)
                 .join(orders_schema->table_name()).on(order_user_id == user_id)
                 .where(order_amount > subquery(avg_order_amount));
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());
    #ifdef MANUAL_CHECK
        std::cout << result.sql << std::endl;
    #endif
}

// Test subquery with DISTINCT
TEST_F(SubqueryTest, SubqueryWithDistinctExpression) {
    auto unique_publishers = select_distinct(post_user_id)
                             .from(posts_schema)
                             .where(post_published == lit(true));
    
    auto query = select(user_name)
                 .from(users_schema)
                 .where(in(user_id, subquery(unique_publishers)));
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());
    #ifdef MANUAL_CHECK
        std::cout << result.sql << std::endl;
    #endif
}