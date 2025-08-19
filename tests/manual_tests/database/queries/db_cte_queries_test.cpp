// CTE (Common Table Expression) tests
// Comprehensive tests for WITH clauses and CTEs

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

// Test fixture for CTE operations
class CteQueryTest : public ::testing::Test,
                     public demiplane::scroll::FileLoggerProvider {
protected:
    void SetUp() override {
        demiplane::scroll::FileLoggerConfig cfg;
        cfg.file             = "query_test.log";
        cfg.add_time_to_filename = false;

        std::shared_ptr<demiplane::scroll::FileLogger<demiplane::scroll::DetailedEntry>> logger = std::make_shared<
            demiplane::scroll::FileLogger<demiplane::scroll::DetailedEntry>>(std::move(cfg));
        set_logger(std::move(logger));
        // Create test schemas
        employees_schema = std::make_shared<TableSchema>("employees");
        employees_schema->add_field<int>("id", "INTEGER")
                        .primary_key("id")
                        .add_field<std::string>("name", "VARCHAR(255)")
                        .add_field<int>("manager_id", "INTEGER")
                        .add_field<std::string>("department", "VARCHAR(100)")
                        .add_field<double>("salary", "DECIMAL(10,2)")
                        .add_field<bool>("active", "BOOLEAN");

        sales_schema = std::make_shared<TableSchema>("sales");
        sales_schema->add_field<int>("id", "INTEGER")
                    .primary_key("id")
                    .add_field<int>("employee_id", "INTEGER")
                    .add_field<double>("amount", "DECIMAL(10,2)")
                    .add_field<std::string>("region", "VARCHAR(50)")
                    .add_field<std::string>("date", "DATE");

        // Create column references
        emp_id         = employees_schema->column<int>("id");
        emp_name       = employees_schema->column<std::string>("name");
        emp_manager_id = employees_schema->column<int>("manager_id");
        emp_department = employees_schema->column<std::string>("department");
        emp_salary     = employees_schema->column<double>("salary");
        emp_active     = employees_schema->column<bool>("active");

        sale_id          = sales_schema->column<int>("id");
        sale_employee_id = sales_schema->column<int>("employee_id");
        sale_amount      = sales_schema->column<double>("amount");
        sale_region      = sales_schema->column<std::string>("region");
        sale_date        = sales_schema->column<std::string>("date");

        // Create compiler
        compiler = std::make_unique<QueryCompiler>(std::make_unique<PostgresDialect>(), false);
    }

    std::shared_ptr<TableSchema> employees_schema;
    std::shared_ptr<TableSchema> sales_schema;

    TableColumn<int> emp_id{nullptr, ""};
    TableColumn<std::string> emp_name{nullptr, ""};
    TableColumn<int> emp_manager_id{nullptr, ""};
    TableColumn<std::string> emp_department{nullptr, ""};
    TableColumn<double> emp_salary{nullptr, ""};
    TableColumn<bool> emp_active{nullptr, ""};

    TableColumn<int> sale_id{nullptr, ""};
    TableColumn<int> sale_employee_id{nullptr, ""};
    TableColumn<double> sale_amount{nullptr, ""};
    TableColumn<std::string> sale_region{nullptr, ""};
    TableColumn<std::string> sale_date{nullptr, ""};

    std::unique_ptr<QueryCompiler> compiler;
};

// Test basic CTE
TEST_F(CteQueryTest, BasicCteExpression) {
    auto high_performers = with("high_performers",
                                select(emp_id, emp_name, emp_salary)
                                .from(employees_schema)
                                .where(emp_salary > lit(75000.0) && emp_active == lit(true)));
    auto result = compiler->compile(high_performers);
    EXPECT_FALSE(result.sql.empty());
    SCROLL_LOG_INF() << "Basic CTE: " << result.sql;
}

// Test CTE with aggregation
TEST_F(CteQueryTest, CteWithAggregationExpression) {
    auto dept_stats = with("dept_stats",
                           select(emp_department.as("department"),
                                  count(emp_id).as("employee_count"),
                                  avg(emp_salary).as("avg_salary"),
                                  max(emp_salary).as("max_salary"))
                           .from(employees_schema)
                           .where(emp_active == lit(true))
                           .group_by(emp_department));
    auto result = compiler->compile(dept_stats);
    EXPECT_FALSE(result.sql.empty());
    SCROLL_LOG_INF() << "CTE with aggregation: " << result.sql;
}

// Test CTE used in main query
TEST_F(CteQueryTest, CteUsedInMainQueryExpression) {
    auto high_earners = with("high_earners",
                             select(emp_id, emp_name)
                             .from(employees_schema)
                             .where(emp_salary > lit(80000.0)));

    // Use CTE in main query
    auto main_query = select(emp_name.as("employee_name").as_dynamic().set_context(high_earners.name()), sale_amount)
                      .from(high_earners)
                      .join(sales_schema).on(sale_employee_id == (emp_id.as_dynamic().set_context(high_earners.name())))
                      .where(sale_amount > lit(10000.0));

    auto result = compiler->compile(main_query);
    EXPECT_FALSE(result.sql.empty());
    SCROLL_LOG_INF() << "CTE used in main query: " << result.sql;
}

// Test multiple CTEs
TEST_F(CteQueryTest, MultipleCteExpression) {
    auto active_employees = with("active_employees",
                                 select(emp_id, emp_name, emp_department)
                                 .from(employees_schema)
                                 .where(emp_active == lit(true)));

    auto high_sales = with("high_sales",
                           select(sale_employee_id, sum(sale_amount).as("total_sales"))
                           .from(sales_schema)
                           .group_by(sale_employee_id)
                           .having(sum(sale_amount) > lit(50000.0)));

    auto main_query = select(emp_name.as_dynamic().set_context(high_sales.name()), emp_department, lit("total_sales"))
        .from(active_employees);
    auto result = compiler->compile(main_query);
    EXPECT_FALSE(result.sql.empty());
    SCROLL_LOG_INF() << "Multiple CTE: " << result.sql;
}


// Test CTE with complex joins
TEST_F(CteQueryTest, CteWithComplexJoinsExpression) {
    auto employee_sales_summary = with("employee_sales_summary",
                                       select(emp_name, emp_department,
                                              sum(sale_amount).as("total_sales"),
                                              count(sale_id).as("sale_count"))
                                       .from(employees_schema)
                                       .join(sales_schema->table_name(), JoinType::LEFT).on(sale_employee_id == emp_id)
                                       .where(emp_active == lit(true))
                                       .group_by(emp_id, emp_name, emp_department));

    auto result = compiler->compile(employee_sales_summary);
    EXPECT_FALSE(result.sql.empty());
    #ifdef MANUAL_CHECK
    std::cout << result.sql << std::endl;
    #endif
}

// Test CTE with subqueries
TEST_F(CteQueryTest, CteWithSubqueriesExpression) {
    auto top_performers = with("top_performers",
                               select(emp_id, emp_name)
                               .from(employees_schema)
                               .where(emp_salary > subquery(
                                          select(avg(emp_salary))
                                          .from(employees_schema)
                                          .where(emp_active == lit(true))
                                      )));

    auto result = compiler->compile(top_performers);
    EXPECT_FALSE(result.sql.empty());
    SCROLL_LOG_INF() << "CTE with subqueries: " << result.sql;
}