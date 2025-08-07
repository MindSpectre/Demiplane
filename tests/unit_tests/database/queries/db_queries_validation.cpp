// Validation tests for all query expression types
// Tests basic compilation and functionality of each expression

#include <gtest/gtest.h>

#include "query_expressions.hpp"
#include "db_column.hpp"
#include "db_field_schema.hpp"
#include "db_table_schema.hpp"
#include "postgres_dialect.hpp"
#include "query_compiler.hpp"

using namespace demiplane::db;

#define MANUAL_CHECK

// Test fixture with common setup
class QueryValidationTest : public ::testing::Test {
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

// Test SELECT expression with various selectable types
TEST_F(QueryValidationTest, SelectExpression) {
    auto query = select(user_id, user_name);
    auto result = compiler->compile(query.from(users_schema));
    EXPECT_FALSE(result.sql.empty());
    #ifdef MANUAL_CHECK
        std::cout << result.sql << std::endl;
    #endif
}

// Test SELECT with ALL columns
TEST_F(QueryValidationTest, SelectAllColumnsExpression) {
    auto query = select(all("users"));
    auto result = compiler->compile(query.from(users_schema));
    EXPECT_FALSE(result.sql.empty());
    #ifdef MANUAL_CHECK
        std::cout << result.sql << std::endl;
    #endif
}

// Test SELECT DISTINCT
TEST_F(QueryValidationTest, SelectDistinctExpression) {
    auto query = select_distinct(user_name, user_age);
    auto result = compiler->compile(query.from(users_schema));
    EXPECT_FALSE(result.sql.empty());
    #ifdef MANUAL_CHECK
        std::cout << result.sql << std::endl;
    #endif
}

// Test SELECT with mixed types (columns, literals, aggregates)
TEST_F(QueryValidationTest, SelectMixedTypesExpression) {
    auto query = select(user_name, lit("constant"), count(user_id).as("total"));
    auto result = compiler->compile(query.from(users_schema));
    EXPECT_FALSE(result.sql.empty());
    #ifdef MANUAL_CHECK
        std::cout << result.sql << std::endl;
    #endif
}

// Test SELECT from Record
TEST_F(QueryValidationTest, SelectFromRecordExpression) {
    Record test_record(users_schema);
    test_record.set_field<int>("id", 1);
    test_record.set_field<std::string>("name", "test");
    
    auto query = select(user_name).from(test_record);
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());
    #ifdef MANUAL_CHECK
        std::cout << result.sql << std::endl;
    #endif
}

// Test SELECT from table name string
TEST_F(QueryValidationTest, SelectFromTableNameExpression) {
    auto query = select(lit(1)).from("test_table");
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());
    #ifdef MANUAL_CHECK
        std::cout << result.sql << std::endl;
    #endif
}

// Test WHERE expression
TEST_F(QueryValidationTest, WhereExpression) {
    auto query = select(user_name).from(users_schema).where(user_age > lit(18));
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());
    #ifdef MANUAL_CHECK
        std::cout << result.sql << std::endl;
    #endif
}

// Test JOIN expression
TEST_F(QueryValidationTest, JoinExpression) {
    auto query = select(user_name, post_title)
                 .from(users_schema)
                 .join(posts_schema->table_name()).on(post_user_id == user_id);
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());
    #ifdef MANUAL_CHECK
        std::cout << result.sql << std::endl;
    #endif
}

// Test GROUP BY expression
TEST_F(QueryValidationTest, GroupByExpression) {
    auto query = select(user_active, count(user_id).as("user_count"))
                 .from(users_schema)
                 .group_by(user_active);
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());
    #ifdef MANUAL_CHECK
        std::cout << result.sql << std::endl;
    #endif
}

// Test HAVING expression
TEST_F(QueryValidationTest, HavingExpression) {
    auto query = select(user_active, count(user_id).as("user_count"))
                 .from(users_schema)
                 .group_by(user_active)
                 .having(count(user_id) > lit(5));
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());
    #ifdef MANUAL_CHECK
        std::cout << result.sql << std::endl;
    #endif
}

// Test ORDER BY expression
TEST_F(QueryValidationTest, OrderByExpression) {
    auto query = select(user_name)
                 .from(users_schema)
                 .order_by(asc(user_name));
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());
    #ifdef MANUAL_CHECK
        std::cout << result.sql << std::endl;
    #endif
}

// Test LIMIT expression
TEST_F(QueryValidationTest, LimitExpression) {
    auto query = select(user_name)
                 .from(users_schema)
                 .limit(10);
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());
    #ifdef MANUAL_CHECK
        std::cout << result.sql << std::endl;
    #endif
}

// Test CASE expression
TEST_F(QueryValidationTest, CaseExpression) {
    auto query = select(user_name,
                       case_when(user_age < lit(18), lit("minor"))
                       .when(user_age < lit(65), lit("adult"))
                       .else_(lit("senior")))
                 .from(users_schema);
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());
    #ifdef MANUAL_CHECK
        std::cout << result.sql << std::endl;
    #endif
}

// Test BETWEEN expression
TEST_F(QueryValidationTest, BetweenExpression) {
    auto query = select(user_name)
                 .from(users_schema)
                 .where(between(user_age, lit(18), lit(65)));
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());
    #ifdef MANUAL_CHECK
        std::cout << result.sql << std::endl;
    #endif
}

// Test IN LIST expression
TEST_F(QueryValidationTest, InListExpression) {
    auto query = select(user_name)
                 .from(users_schema)
                 .where(in(user_age, lit(18), lit(25), lit(30)));
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());
    #ifdef MANUAL_CHECK
        std::cout << result.sql << std::endl;
    #endif
}

// Test EXISTS expression
TEST_F(QueryValidationTest, ExistExpression) {
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

// Test SUBQUERY expression
TEST_F(QueryValidationTest, SubqueryExpression) {
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

// Test AGGREGATE expressions
TEST_F(QueryValidationTest, AggregateExpressions) {
    // COUNT
    auto count_query = select(count(user_id)).from(users_schema);
    auto count_result = compiler->compile(count_query);
    EXPECT_FALSE(count_result.sql.empty());
    
    // SUM
    auto sum_query = select(sum(user_age)).from(users_schema);
    auto sum_result = compiler->compile(sum_query);
    EXPECT_FALSE(sum_result.sql.empty());
    
    // AVG
    auto avg_query = select(avg(user_age)).from(users_schema);
    auto avg_result = compiler->compile(avg_query);
    EXPECT_FALSE(avg_result.sql.empty());
    
    // MIN
    auto min_query = select(min(user_age)).from(users_schema);
    auto min_result = compiler->compile(min_query);
    EXPECT_FALSE(min_result.sql.empty());
    
    // MAX
    auto max_query = select(max(user_age)).from(users_schema);
    auto max_result = compiler->compile(max_query);
    EXPECT_FALSE(max_result.sql.empty());
}

// Test AGGREGATE with aliases
TEST_F(QueryValidationTest, AggregateWithAliasExpressions) {
    auto query = select(
        count(user_id).as("total_users"),
        sum(user_age).as("total_age"),
        avg(user_age).as("avg_age"),
        min(user_age).as("min_age"),
        max(user_age).as("max_age")
    ).from(users_schema);
    
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());
    #ifdef MANUAL_CHECK
        std::cout << result.sql << std::endl;
    #endif
}

// Test COUNT DISTINCT
TEST_F(QueryValidationTest, CountDistinctExpression) {
    auto query = select(count_distinct(user_age)).from(users_schema);
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());
    #ifdef MANUAL_CHECK
        std::cout << result.sql << std::endl;
    #endif
}

// Test COUNT ALL
TEST_F(QueryValidationTest, CountAllExpression) {
    auto query = select(count_all()).from(users_schema);
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());
    #ifdef MANUAL_CHECK
        std::cout << result.sql << std::endl;
    #endif
}

// Test SET OPERATION expressions
TEST_F(QueryValidationTest, SetOpExpression) {
    auto query1 = select(user_name).from(users_schema).where(user_active == lit(true));
    auto query2 = select(user_name).from(users_schema).where(user_age > lit(65));
    
    auto union_query = union_all(query1, query2);
    auto result = compiler->compile(union_query);
    EXPECT_FALSE(result.sql.empty());
    #ifdef MANUAL_CHECK
        std::cout << result.sql << std::endl;
    #endif
}

// Test CTE expression
TEST_F(QueryValidationTest, CteExpression) {
    auto high_value_users = with("high_value_users",
                                select(user_id, user_name)
                                .from(users_schema)
                                .where(user_active == lit(true) && user_age > lit(25)));
    auto result = compiler->compile(high_value_users);
    EXPECT_FALSE(result.sql.empty());
    #ifdef MANUAL_CHECK
        std::cout << result.sql << std::endl;
    #endif
}

// Test INSERT expression
TEST_F(QueryValidationTest, InsertExpression) {
    auto query = insert_into(users_schema)
                 .into({"name", "age", "active"})
                 .values({"John Doe", 25, true});
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());
    #ifdef MANUAL_CHECK
        std::cout << result.sql << std::endl;
    #endif
}

// Test INSERT with table name string
TEST_F(QueryValidationTest, InsertWithTableNameExpression) {
    auto query = insert_into("users")
                 .into({"name", "age"})
                 .values({"Jane Doe", 30});
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());
    #ifdef MANUAL_CHECK
        std::cout << result.sql << std::endl;
    #endif
}

// Test INSERT with Record
TEST_F(QueryValidationTest, InsertWithRecordExpression) {
    Record test_record(users_schema);
    test_record.set_field<std::string>("name", "Bob Smith");
    test_record.set_field<int>("age", 35);
    test_record.set_field<bool>("active", true);
    
    auto query = insert_into(users_schema)
                 .into({"name", "age", "active"})
                 .values(test_record);
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());
    #ifdef MANUAL_CHECK
        std::cout << result.sql << std::endl;
    #endif
}

// Test INSERT batch operation
TEST_F(QueryValidationTest, InsertBatchExpression) {
    Record record1(users_schema);
    record1.set_field<std::string>("name", "User1");
    record1.set_field<int>("age", 25);
    record1.set_field<bool>("active", true);
    
    Record record2(users_schema);
    record2.set_field<std::string>("name", "User2");
    record2.set_field<int>("age", 30);
    record2.set_field<bool>("active", false);
    
    std::vector<Record> records = {record1, record2};
    
    auto query = insert_into(users_schema)
                 .into({"name", "age", "active"})
                 .batch(records);
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());
    #ifdef MANUAL_CHECK
        std::cout << result.sql << std::endl;
    #endif
}

// Test INSERT multiple values calls
TEST_F(QueryValidationTest, InsertMultipleValuesExpression) {
    auto query = insert_into(users_schema)
                 .into({"name", "age", "active"})
                 .values({"User1", 25, true})
                 .values({"User2", 30, false});
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());
    #ifdef MANUAL_CHECK
        std::cout << result.sql << std::endl;
    #endif
}

// Test UPDATE expression
TEST_F(QueryValidationTest, UpdateExpression) {
    auto query = update(users_schema)
                 .set("active", false)
                 .where(user_age < lit(18));
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());
    #ifdef MANUAL_CHECK
        std::cout << result.sql << std::endl;
    #endif
}

// Test UPDATE with table name string
TEST_F(QueryValidationTest, UpdateWithTableNameExpression) {
    auto query = update("users")
                 .set("active", true)
                 .where(lit(true));
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());
    #ifdef MANUAL_CHECK
        std::cout << result.sql << std::endl;
    #endif
}

// Test UPDATE with multiple set operations
TEST_F(QueryValidationTest, UpdateMultipleSetExpression) {
    auto query = update(users_schema)
                 .set("active", false)
                 .set("age", 21)
                 .where(user_age < lit(18));
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());
    #ifdef MANUAL_CHECK
        std::cout << result.sql << std::endl;
    #endif
}

// Test UPDATE with initializer list set
TEST_F(QueryValidationTest, UpdateInitializerListSetExpression) {
    auto query = update(users_schema)
                 .set({{"active", false}, {"age", 21}})
                 .where(user_age < lit(18));
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());
    #ifdef MANUAL_CHECK
        std::cout << result.sql << std::endl;
    #endif
}

// Test UPDATE without WHERE clause
TEST_F(QueryValidationTest, UpdateWithoutWhereExpression) {
    auto update_query = update(users_schema).set("active", true);
    auto result = compiler->compile(update_query);
    EXPECT_FALSE(result.sql.empty());
    #ifdef MANUAL_CHECK
        std::cout << result.sql << std::endl;
    #endif
}

// Test UPDATE WHERE expression
TEST_F(QueryValidationTest, UpdateWhereExpression) {
    auto update_query = update(users_schema).set("active", false);
    auto query = UpdateWhereExpr{update_query, user_age < lit(18)};
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());
    #ifdef MANUAL_CHECK
        std::cout << result.sql << std::endl;
    #endif
}

// Test DELETE expression
TEST_F(QueryValidationTest, DeleteExpression) {
    auto query = delete_from(users_schema)
                 .where(user_active == lit(false));
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());
    #ifdef MANUAL_CHECK
        std::cout << result.sql << std::endl;
    #endif
}

// Test DELETE with table name string
TEST_F(QueryValidationTest, DeleteWithTableNameExpression) {
    auto query = delete_from("users")
                 .where(lit(true));
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());
    #ifdef MANUAL_CHECK
        std::cout << result.sql << std::endl;
    #endif
}

// Test DELETE without WHERE clause
TEST_F(QueryValidationTest, DeleteWithoutWhereExpression) {
    auto delete_query = delete_from(users_schema);
    auto result = compiler->compile(delete_query);
    EXPECT_FALSE(result.sql.empty());
    #ifdef MANUAL_CHECK
        std::cout << result.sql << std::endl;
    #endif
}

// Test DELETE WHERE expression
TEST_F(QueryValidationTest, DeleteWhereExpression) {
    auto delete_query = delete_from(users_schema);
    auto query = DeleteWhereExpr{delete_query, user_active == lit(false)};
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());
    #ifdef MANUAL_CHECK
        std::cout << result.sql << std::endl;
    #endif
}

// Test FROM expression
TEST_F(QueryValidationTest, FromExpression) {
    auto select_query = select(user_name);
    auto query = FromExpr{select_query, users_schema};
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());
    #ifdef MANUAL_CHECK
        std::cout << result.sql << std::endl;
    #endif
}

// Test CONDITION expression (binary operations)
TEST_F(QueryValidationTest, ConditionExpressions) {
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
}

// Test UNARY condition expressions
TEST_F(QueryValidationTest, UnaryConditionExpressions) {
    // IS NULL
    auto is_null_query = select(user_name).from(users_schema).where(is_null(user_name));
    auto is_null_result = compiler->compile(is_null_query);
    EXPECT_FALSE(is_null_result.sql.empty());
    
    // IS NOT NULL
    auto is_not_null_query = select(user_name).from(users_schema).where(is_not_null(user_name));
    auto is_not_null_result = compiler->compile(is_not_null_query);
    EXPECT_FALSE(is_not_null_result.sql.empty());
    
    // NOT
    auto not_query = select(user_name).from(users_schema).where(!user_active);
    auto not_result = compiler->compile(not_query);
    EXPECT_FALSE(not_result.sql.empty());
    
    #ifdef MANUAL_CHECK
        std::cout << "IS NULL: " << is_null_result.sql << std::endl;
        std::cout << "IS NOT NULL: " << is_not_null_result.sql << std::endl;
        std::cout << "NOT: " << not_result.sql << std::endl;
    #endif
}

// Test LIKE expressions
TEST_F(QueryValidationTest, LikeExpressions) {
    // LIKE
    auto like_query = select(user_name).from(users_schema).where(like(user_name, lit("%john%")));
    auto like_result = compiler->compile(like_query);
    EXPECT_FALSE(like_result.sql.empty());
    
    // NOT LIKE
    auto not_like_query = select(user_name).from(users_schema).where(not_like(user_name, lit("%admin%")));
    auto not_like_result = compiler->compile(not_like_query);
    EXPECT_FALSE(not_like_result.sql.empty());
    
    #ifdef MANUAL_CHECK
        std::cout << "LIKE: " << like_result.sql << std::endl;
        std::cout << "NOT LIKE: " << not_like_result.sql << std::endl;
    #endif
}