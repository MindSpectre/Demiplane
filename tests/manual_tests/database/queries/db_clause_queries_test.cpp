// SQL clause tests (WHERE, FROM, GROUP BY, HAVING, ORDER BY, LIMIT)
// Comprehensive tests for SQL clause expressions

#include <gtest/gtest.h>

#include "query_expressions.hpp"
#include "db_column.hpp"
#include "db_field_schema.hpp"
#include "db_table_schema.hpp"
#include "postgres_dialect.hpp"
#include "query_compiler.hpp"

#include <demiplane/scroll>

using namespace demiplane::db;

#define MANUAL_CHECK

// Test fixture for SQL clause operations
class ClauseQueryTest : public ::testing::Test,
                        public demiplane::scroll::LoggerProvider {
protected:
    void SetUp() override {
        demiplane::scroll::FileLoggerConfig cfg;
        cfg.file                 = "query_test.log";
        cfg.add_time_to_filename = false;

        std::shared_ptr<demiplane::scroll::FileLogger<demiplane::scroll::DetailedEntry>> logger = std::make_shared<
            demiplane::scroll::FileLogger<demiplane::scroll::DetailedEntry>>(std::move(cfg));
        set_logger(std::move(logger));
        // Create test schemas
        users_schema = std::make_shared<TableSchema>("users");
        users_schema->add_field<int>("id", "INTEGER")
                    .primary_key("id")
                    .add_field<std::string>("name", "VARCHAR(255)")
                    .add_field<int>("age", "INTEGER")
                    .add_field<bool>("active", "BOOLEAN")
                    .add_field<std::string>("department", "VARCHAR(100)")
                    .add_field<double>("salary", "DECIMAL(10,2)");

        orders_schema = std::make_shared<TableSchema>("orders");
        orders_schema->add_field<int>("id", "INTEGER")
                     .primary_key("id")
                     .add_field<int>("user_id", "INTEGER")
                     .add_field<double>("amount", "DECIMAL(10,2)")
                     .add_field<std::string>("status", "VARCHAR(50)")
                     .add_field<std::string>("created_date", "DATE");

        // Create column references
        user_id         = users_schema->column<int>("id");
        user_name       = users_schema->column<std::string>("name");
        user_age        = users_schema->column<int>("age");
        user_active     = users_schema->column<bool>("active");
        user_department = users_schema->column<std::string>("department");
        user_salary     = users_schema->column<double>("salary");

        order_id           = orders_schema->column<int>("id");
        order_user_id      = orders_schema->column<int>("user_id");
        order_amount       = orders_schema->column<double>("amount");
        order_status       = orders_schema->column<std::string>("status");
        order_created_date = orders_schema->column<std::string>("created_date");

        // Create compiler
        compiler = std::make_unique<QueryCompiler>(std::make_unique<PostgresDialect>(), false);
    }

    std::shared_ptr<TableSchema> users_schema;
    std::shared_ptr<TableSchema> orders_schema;

    TableColumn<int> user_id{nullptr, ""};
    TableColumn<std::string> user_name{nullptr, ""};
    TableColumn<int> user_age{nullptr, ""};
    TableColumn<bool> user_active{nullptr, ""};
    TableColumn<std::string> user_department{nullptr, ""};
    TableColumn<double> user_salary{nullptr, ""};

    TableColumn<int> order_id{nullptr, ""};
    TableColumn<int> order_user_id{nullptr, ""};
    TableColumn<double> order_amount{nullptr, ""};
    TableColumn<std::string> order_status{nullptr, ""};
    TableColumn<std::string> order_created_date{nullptr, ""};

    std::unique_ptr<QueryCompiler> compiler;
};

// Test FROM clause variations
TEST_F(ClauseQueryTest, FromClauseExpression) {
    // FROM with TableSchema
    auto query1  = select(user_name).from(users_schema);
    auto result1 = compiler->compile(query1);
    EXPECT_FALSE(result1.sql.empty());

    // FROM with table name string
    auto query2  = select(lit(1)).from("test_table");
    auto result2 = compiler->compile(query2);
    EXPECT_FALSE(result2.sql.empty());

    SCROLL_LOG_INF() << "FROM schema: " << result1.sql;
    SCROLL_LOG_INF() << "FROM string: " << result2.sql;
}

// Test WHERE clause with various conditions
TEST_F(ClauseQueryTest, WhereClauseExpression) {
    // Simple WHERE
    auto query1  = select(user_name).from(users_schema).where(user_active == lit(true));
    auto result1 = compiler->compile(query1);
    EXPECT_FALSE(result1.sql.empty());

    // WHERE with AND/OR
    auto query2 = select(user_name)
                  .from(users_schema)
                  .where(user_age > lit(18) && (user_active == lit(true) || user_salary > lit(50000.0)));
    auto result2 = compiler->compile(query2);
    EXPECT_FALSE(result2.sql.empty());

    // WHERE with IN
    auto query3 = select(user_name)
                  .from(users_schema)
                  .where(in(user_age, lit(25), lit(30), lit(35)));
    auto result3 = compiler->compile(query3);
    EXPECT_FALSE(result3.sql.empty());

    // WHERE with BETWEEN
    auto query4 = select(user_name)
                  .from(users_schema)
                  .where(between(user_salary, lit(30000.0), lit(80000.0)));
    auto result4 = compiler->compile(query4);
    EXPECT_FALSE(result4.sql.empty());

    SCROLL_LOG_INF() << "WHERE simple: " << result1.sql;
    SCROLL_LOG_INF() << "WHERE complex: " << result2.sql;
    SCROLL_LOG_INF() << "WHERE IN: " << result3.sql;
    SCROLL_LOG_INF() << "WHERE BETWEEN: " << result4.sql;
}

// Test GROUP BY clause
TEST_F(ClauseQueryTest, GroupByClauseExpression) {
    // Single column GROUP BY
    auto query1 = select(user_department, count(user_id).as("count"))
                  .from(users_schema)
                  .group_by(user_department);
    auto result1 = compiler->compile(query1);
    EXPECT_FALSE(result1.sql.empty());

    // Multiple column GROUP BY
    auto query2 = select(user_department, user_active, count(user_id).as("count"))
                  .from(users_schema)
                  .group_by(user_department, user_active);
    auto result2 = compiler->compile(query2);
    EXPECT_FALSE(result2.sql.empty());

    // GROUP BY with WHERE
    auto query3 = select(user_department, avg(user_salary).as("avg_salary"))
                  .from(users_schema)
                  .where(user_active == lit(true))
                  .group_by(user_department);
    auto result3 = compiler->compile(query3);
    EXPECT_FALSE(result3.sql.empty());

    SCROLL_LOG_INF() << "GROUP BY single: " << result1.sql;
    SCROLL_LOG_INF() << "GROUP BY multiple: " << result2.sql;
    SCROLL_LOG_INF() << "GROUP BY with WHERE: " << result3.sql;
}

// Test HAVING clause
TEST_F(ClauseQueryTest, HavingClauseExpression) {
    // HAVING with aggregate condition
    auto query1 = select(user_department, count(user_id).as("count"))
                  .from(users_schema)
                  .group_by(user_department)
                  .having(count(user_id) > lit(5));
    auto result1 = compiler->compile(query1);
    EXPECT_FALSE(result1.sql.empty());

    // HAVING with multiple conditions
    auto query2 = select(user_department, avg(user_salary).as("avg_salary"), count(user_id).as("count"))
                  .from(users_schema)
                  .group_by(user_department)
                  .having(count(user_id) > lit(3) && avg(user_salary) > lit(45000.0));
    auto result2 = compiler->compile(query2);
    EXPECT_FALSE(result2.sql.empty());

    // HAVING with WHERE and GROUP BY
    auto query3 = select(user_department, max(user_salary).as("max_salary"))
                  .from(users_schema)
                  .where(user_active == lit(true))
                  .group_by(user_department)
                  .having(max(user_salary) > lit(70000.0));
    auto result3 = compiler->compile(query3);
    EXPECT_FALSE(result3.sql.empty());

    SCROLL_LOG_INF() << "HAVING simple: " << result1.sql;
    SCROLL_LOG_INF() << "HAVING multiple: " << result2.sql;
    SCROLL_LOG_INF() << "HAVING with WHERE/GROUP BY: " << result3.sql;
}

// Test ORDER BY clause
TEST_F(ClauseQueryTest, OrderByClauseExpression) {
    // Single column ORDER BY ASC
    auto query1 = select(user_name, user_age)
                  .from(users_schema)
                  .order_by(asc(user_name));
    auto result1 = compiler->compile(query1);
    EXPECT_FALSE(result1.sql.empty());

    // Single column ORDER BY DESC
    auto query2 = select(user_name, user_salary)
                  .from(users_schema)
                  .order_by(desc(user_salary));
    auto result2 = compiler->compile(query2);
    EXPECT_FALSE(result2.sql.empty());

    // Multiple column ORDER BY
    auto query3 = select(user_name, user_department, user_salary)
                  .from(users_schema)
                  .order_by(asc(user_department), desc(user_salary), asc(user_name));
    auto result3 = compiler->compile(query3);
    EXPECT_FALSE(result3.sql.empty());

    // ORDER BY with expressions
    auto query4 = select(user_name, user_age, user_salary)
                  .from(users_schema)
                  .order_by(desc(user_salary));
    auto result4 = compiler->compile(query4);
    EXPECT_FALSE(result4.sql.empty());

    SCROLL_LOG_INF() << "ORDER BY ASC: " << result1.sql;
    SCROLL_LOG_INF() << "ORDER BY DESC: " << result2.sql;
    SCROLL_LOG_INF() << "ORDER BY multiple: " << result3.sql;
    SCROLL_LOG_INF() << "ORDER BY expression: " << result4.sql;
}

// Test LIMIT clause
TEST_F(ClauseQueryTest, LimitClauseExpression) {
    // Basic LIMIT
    auto query1 = select(user_name)
                  .from(users_schema)
                  .limit(10);
    auto result1 = compiler->compile(query1);
    EXPECT_FALSE(result1.sql.empty());

    // LIMIT with ORDER BY
    auto query2 = select(user_name, user_salary)
                  .from(users_schema)
                  .order_by(desc(user_salary))
                  .limit(5);
    auto result2 = compiler->compile(query2);
    EXPECT_FALSE(result2.sql.empty());

    // LIMIT with WHERE and ORDER BY
    auto query3 = select(user_name, user_age)
                  .from(users_schema)
                  .where(user_active == lit(true))
                  .order_by(asc(user_age))
                  .limit(20);
    auto result3 = compiler->compile(query3);
    EXPECT_FALSE(result3.sql.empty());

    SCROLL_LOG_INF() << "LIMIT basic: " << result1.sql;
    SCROLL_LOG_INF() << "LIMIT with ORDER BY: " << result2.sql;
    SCROLL_LOG_INF() << "LIMIT with WHERE/ORDER BY: " << result3.sql;
}

// Test complex query with all clauses
TEST_F(ClauseQueryTest, ComplexQueryWithAllClausesExpression) {
    auto query = select(user_department,
                        count(user_id).as("employee_count"),
                        avg(user_salary).as("avg_salary"),
                        max(user_salary).as("max_salary"))
                 .from(users_schema)
                 .where(user_active == lit(true) && user_age >= lit(21))
                 .group_by(user_department)
                 .having(count(user_id) >= lit(3) && avg(user_salary) > lit(40000.0))
                 .order_by(desc(/*avg*/(user_salary)), asc(user_department))
                 .limit(10);
    //todo: desc accept aggreagate
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());
    SCROLL_LOG_INF() << "Complex query: " << result.sql;
}

// Test clause combinations with JOINs
TEST_F(ClauseQueryTest, ClausesWithJoinsExpression) {
    auto query = select(user_name, user_department, sum(order_amount).as("total_orders"))
                 .from(users_schema)
                 .join(orders_schema->table_name()).on(order_user_id == user_id)
                 .where(user_active == lit(true) && order_status == lit("completed"))
                 .group_by(user_id, user_name, user_department)
                 .having(sum(order_amount) > lit(1000.0))
                 .order_by(desc(/*sum*/(order_amount)))
                 .limit(5);

    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());
    SCROLL_LOG_INF() << "Clauses with JOIN: " << result.sql;
}