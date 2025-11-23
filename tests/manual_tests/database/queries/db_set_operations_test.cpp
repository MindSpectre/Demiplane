// SET operations tests
// Comprehensive tests for UNION, INTERSECT, EXCEPT operations

#include <demiplane/scroll>

#include "db_column.hpp"
#include "db_field_schema.hpp"
#include "db_table_schema.hpp"
#include "postgres_dialect.hpp"
#include "query_compiler.hpp"
#include "query_expressions.hpp"

#include <gtest/gtest.h>

using namespace demiplane::db;

#define MANUAL_CHECK

// Test fixture for SET operations
class SetOperationsTest : public ::testing::Test, public demiplane::scroll::LoggerProvider {
protected:
    void SetUp() override {
        demiplane::scroll::FileSinkConfig cfg;
        cfg.file                 = "query_test.log";
        cfg.add_time_to_filename = false;

        auto logger = std::make_shared<demiplane::scroll::Logger>();
        auto file_sink = std::make_shared<demiplane::scroll::FileSink<demiplane::scroll::DetailedEntry>>(std::move(cfg));
        logger->add_sink(std::move(file_sink));
        set_logger(std::move(logger));
        // Create test schemas
        users_schema = std::make_shared<TableSchema>("users");
        users_schema->add_field<int>("id", "INTEGER")
            .primary_key("id")
            .add_field<std::string>("name", "VARCHAR(255)")
            .add_field<int>("age", "INTEGER")
            .add_field<bool>("active", "BOOLEAN")
            .add_field<std::string>("department", "VARCHAR(100)");

        employees_schema = std::make_shared<TableSchema>("employees");
        employees_schema->add_field<int>("id", "INTEGER")
            .primary_key("id")
            .add_field<std::string>("name", "VARCHAR(255)")
            .add_field<int>("age", "INTEGER")
            .add_field<std::string>("department", "VARCHAR(100)")
            .add_field<double>("salary", "DECIMAL(10,2)");

        // Create column references
        user_id         = users_schema->column<int>("id");
        user_name       = users_schema->column<std::string>("name");
        user_age        = users_schema->column<int>("age");
        user_active     = users_schema->column<bool>("active");
        user_department = users_schema->column<std::string>("department");

        emp_id         = employees_schema->column<int>("id");
        emp_name       = employees_schema->column<std::string>("name");
        emp_age        = employees_schema->column<int>("age");
        emp_department = employees_schema->column<std::string>("department");
        emp_salary     = employees_schema->column<double>("salary");

        // Create compiler
        compiler = std::make_unique<QueryCompiler>(std::make_unique<PostgresDialect>(), false);
    }

    std::shared_ptr<TableSchema> users_schema;
    std::shared_ptr<TableSchema> employees_schema;

    TableColumn<int> user_id{nullptr, ""};
    TableColumn<std::string> user_name{nullptr, ""};
    TableColumn<int> user_age{nullptr, ""};
    TableColumn<bool> user_active{nullptr, ""};
    TableColumn<std::string> user_department{nullptr, ""};

    TableColumn<int> emp_id{nullptr, ""};
    TableColumn<std::string> emp_name{nullptr, ""};
    TableColumn<int> emp_age{nullptr, ""};
    TableColumn<std::string> emp_department{nullptr, ""};
    TableColumn<double> emp_salary{nullptr, ""};

    std::unique_ptr<QueryCompiler> compiler;
};

// Test UNION operation
TEST_F(SetOperationsTest, UnionExpression) {
    const auto active_users =
        select(user_name.as("name"), user_age.as("age")).from(users_schema).where(user_active == lit(true));

    const auto young_employees =
        select(emp_name.as("name"), emp_age.as("age")).from(employees_schema).where(emp_age < lit(30));

    auto query  = union_query(active_users, young_employees);
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());
    LOG_INF() << result.sql;
}

// Test UNION ALL operation
TEST_F(SetOperationsTest, UnionAllExpression) {
    auto all_users = select(user_name.as("name")).from(users_schema);

    auto all_employees = select(emp_name.as("name")).from(employees_schema);

    auto query  = union_all(all_users, all_employees);
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());
    LOG_INF() << result.sql;
}

// Test INTERSECT operation
TEST_F(SetOperationsTest, IntersectExpression) {
    auto it_users = select(user_name.as("name")).from(users_schema).where(user_department == lit("IT"));

    auto it_employees = select(emp_name.as("name")).from(employees_schema).where(emp_department == lit("IT"));

    auto query  = intersect(it_users, it_employees);
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());
    LOG_INF() << result.sql;
}

// Test EXCEPT operation
TEST_F(SetOperationsTest, ExceptExpression) {
    auto all_user_names = select(user_name.as("name")).from(users_schema);

    auto inactive_user_names = select(user_name.as("name")).from(users_schema).where(user_active == lit(false));

    auto query  = except(all_user_names, inactive_user_names);
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());
    LOG_INF() << result.sql;
}

// Test multiple UNION operations
TEST_F(SetOperationsTest, MultipleUnionExpression) {
    const auto young_users =
        select(user_name.as("name"), lit("User").as("type")).from(users_schema).where(user_age < lit(25));

    const auto senior_employees =
        select(emp_name.as("name"), lit("Employee").as("type")).from(employees_schema).where(emp_age > lit(50));

    const auto high_salary_employees = select(emp_name.as("name"), lit("High Earner").as("type"))
                                           .from(employees_schema)
                                           .where(emp_salary > lit(75000.0));

    auto query  = union_all(union_all(young_users, senior_employees), high_salary_employees);
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());
    LOG_INF() << result.sql;
}

// Test SET operation with ORDER BY
TEST_F(SetOperationsTest, SetOperationWithOrderByExpression) {
    const auto active_users =
        select(user_name.as("name"), user_age.as("age")).from(users_schema).where(user_active == lit(true));

    const auto employees = select(emp_name.as("name"), emp_age.as("age")).from(employees_schema);

    const auto als_name = user_name.as_dynamic().set_name("name");
    const auto als_age  = user_age.as_dynamic().set_name("age");
    auto query          = union_all(active_users, employees).order_by(asc(als_name), desc(als_age));
    auto result         = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());
    LOG_INF() << result.sql;
}

// Test SET operation with LIMIT
TEST_F(SetOperationsTest, SetOperationWithLimitExpression) {
    auto users = select(user_name.as("name")).from(users_schema);

    auto employees = select(emp_name.as("name")).from(employees_schema);

    auto query  = union_all(users, employees).limit(10);
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());
    LOG_INF() << result.sql;
}

// Test SET operation with different column counts (should match)
TEST_F(SetOperationsTest, SetOperationMatchingColumnsExpression) {
    const auto user_summary = select(user_name.as("name"), user_department.as("dept"), lit("Active User").as("status"))
                                  .from(users_schema)
                                  .where(user_active == lit(true));

    const auto employee_summary =
        select(emp_name.as("name"), emp_department.as("dept"), lit("Employee").as("status")).from(employees_schema);

    auto query  = union_all(user_summary, employee_summary);
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());
    LOG_INF() << result.sql;
}

// Test complex SET operations with subqueries
TEST_F(SetOperationsTest, ComplexSetOperationsWithSubqueriesExpression) {
    const auto dept_users = select(user_department.as("department"), count(user_id).as("count"))
                                .from(users_schema)
                                .where(user_active == lit(true))
                                .group_by(user_department);

    const auto dept_employees = select(emp_department.as("department"), count(emp_id).as("count"))
                                    .from(employees_schema)
                                    .group_by(emp_department);

    auto query  = union_all(dept_users, dept_employees);
    auto result = compiler->compile(query);
    EXPECT_FALSE(result.sql.empty());
    LOG_INF() << result.sql;
}
