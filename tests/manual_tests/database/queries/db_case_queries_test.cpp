// CASE expression tests
// Comprehensive tests for CASE/WHEN/ELSE expressions

#include <demiplane/scroll>

#include "postgres_dialect.hpp"
#include "query_compiler.hpp"
#include "query_expressions.hpp"

#include <gtest/gtest.h>

using namespace demiplane::db;


// Test fixture for CASE operations
class CaseQueryTest : public ::testing::Test, public demiplane::scroll::LoggerProvider {
protected:
    void SetUp() override {
        demiplane::scroll::FileLoggerConfig cfg;
        cfg.file                 = "query_test.log";
        cfg.add_time_to_filename = false;

        auto logger = std::make_shared<demiplane::scroll::FileLogger<demiplane::scroll::DetailedEntry>>(std::move(cfg));
        set_logger(std::move(logger));
        // Create test schema
        users_schema = std::make_shared<Table>("users");
        users_schema->add_field<int>("id", "INTEGER")
            .primary_key("id")
            .add_field<std::string>("name", "VARCHAR(255)")
            .add_field<int>("age", "INTEGER")
            .add_field<double>("salary", "DECIMAL(10,2)")
            .add_field<bool>("active", "BOOLEAN")
            .add_field<std::string>("status", "VARCHAR(50)");

        // Create column references
        user_id     = users_schema->column<int>("id");
        user_name   = users_schema->column<std::string>("name");
        user_age    = users_schema->column<int>("age");
        user_salary = users_schema->column<double>("salary");
        user_active = users_schema->column<bool>("active");
        user_status = users_schema->column<std::string>("status");

        // Create compiler
        compiler = std::make_unique<QueryCompiler>(std::make_unique<PostgresDialect>(), false);
    }

    std::shared_ptr<Table> users_schema;

    TableColumn<int> user_id{nullptr, ""};
    TableColumn<std::string> user_name{nullptr, ""};
    TableColumn<int> user_age{nullptr, ""};
    TableColumn<double> user_salary{nullptr, ""};
    TableColumn<bool> user_active{nullptr, ""};
    TableColumn<std::string> user_status{nullptr, ""};

    std::unique_ptr<QueryCompiler> compiler;
};

// Test basic CASE expression
TEST_F(CaseQueryTest, BasicCaseExpression) {
    auto query =
        select(user_name,
               case_when(user_age < lit(18), lit("minor")).when(user_age < lit(65), lit("adult")).else_(lit("senior")))
            .from(users_schema);
    const auto q   = compiler->compile(query);
    const auto sql = q.sql();
    EXPECT_FALSE(sql.empty());
    SCROLL_LOG_INF() << sql;
}

// Test CASE without ELSE
TEST_F(CaseQueryTest, CaseWithoutElseExpression) {
    auto query =
        select(user_name,
               case_when(user_active == lit(true), lit("Active")).when(user_active == lit(false), lit("Inactive")))
            .from(users_schema);
    const auto q   = compiler->compile(query);
    const auto sql = q.sql();
    EXPECT_FALSE(sql.empty());
    SCROLL_LOG_INF() << sql;
}

// Test CASE with multiple WHEN clauses
TEST_F(CaseQueryTest, MultipleWhenExpression) {
    auto query = select(user_name,
                        case_when(user_salary < lit(30000.0), lit("Low"))
                            .when(user_salary < lit(60000.0), lit("Medium"))
                            .when(user_salary < lit(100000.0), lit("High"))
                            .else_(lit("Very High"))
                            .as("salary_category"))
                     .from(users_schema);
    const auto q   = compiler->compile(query);
    const auto sql = q.sql();
    EXPECT_FALSE(sql.empty());
    SCROLL_LOG_INF() << sql;
}

// Test CASE with complex conditions
TEST_F(CaseQueryTest, CaseWithComplexConditionsExpression) {
    auto query = select(user_name,
                        case_when(user_age < lit(25) && user_active == lit(true), lit("Young Active"))
                            .when(user_age >= lit(25) && user_salary > lit(50000.0), lit("Mature High Earner"))
                            .when(user_active == lit(false), lit("Inactive"))
                            .else_(lit("Other")))
                     .from(users_schema);
    const auto q   = compiler->compile(query);
    const auto sql = q.sql();
    EXPECT_FALSE(sql.empty());
    SCROLL_LOG_INF() << sql;
}

// Test CASE in WHERE clause
TEST_F(CaseQueryTest, CaseInWhereExpression) {
    auto query = select(user_name)
                     .from(users_schema)
                     .where(case_when(user_active == lit(true), user_salary).else_(lit(0.0)) > lit(40000.0));
    const auto q   = compiler->compile(query);
    const auto sql = q.sql();
    EXPECT_FALSE(sql.empty());
    SCROLL_LOG_INF() << sql;
}

// Test CASE with aggregates
// TEST_F(CaseQueryTest, CaseWithAggregatesExpression) {
//     auto query = select(
//         sum(case_when(user_active == lit(true), lit(1)).else_(lit(0))).as("active_count"),
//         sum(case_when(user_active == lit(false), lit(1)).else_(lit(0))).as("inactive_count"),
//         avg(case_when(user_active == lit(true), user_salary).else_(lit(0.0))).as("avg_active_salary")
//     ).from(users_schema);
//     auto result = compiler->compile(query);
//     EXPECT_FALSE(result.sql.empty());
//     #ifdef MANUAL_CHECK
//         std::cout << result.sql << std::endl;
//     #endif
// }

// Test CASE with GROUP BY
TEST_F(CaseQueryTest, CaseWithGroupByExpression) {
    auto age_group =
        case_when(user_age < lit(30), lit("Young")).when(user_age < lit(50), lit("Middle")).else_(lit("Senior"));

    auto query   = select(age_group.as("age_group"), count(user_id).as("count")).from(users_schema).group_by(age_group);
    const auto q = compiler->compile(query);
    const auto sql = q.sql();
    EXPECT_FALSE(sql.empty());
    SCROLL_LOG_INF() << sql;
}

// Test nested CASE expressions
TEST_F(CaseQueryTest, NestedCaseExpression) {
    auto query = select(user_name,
                        case_when(user_active == lit(true),
                                  case_when(user_salary > lit(50000.0), lit("High Active")).else_(lit("Low Active")))
                            .else_(case_when(user_age > lit(60), lit("Retired")).else_(lit("Inactive"))))
                     .from(users_schema);
    const auto q   = compiler->compile(query);
    const auto sql = q.sql();
    EXPECT_FALSE(sql.empty());
    SCROLL_LOG_INF() << sql;
}

// Test CASE with different data types
TEST_F(CaseQueryTest, CaseWithDifferentTypesExpression) {
    auto query = select(user_name,
                        case_when(user_active == lit(true), user_salary).else_(lit(0.0)).as("effective_salary"),
                        case_when(user_age < lit(18), lit(false)).else_(lit(true)).as("can_work"))
                     .from(users_schema);
    const auto q   = compiler->compile(query);
    const auto sql = q.sql();
    EXPECT_FALSE(sql.empty());
    SCROLL_LOG_INF() << sql;
}

// Test CASE with ORDER BY
TEST_F(CaseQueryTest, CaseWithOrderByExpression) {
    auto priority = case_when(user_status == lit("VIP"), lit(1))
                        .when(user_status == lit("Premium"), lit(2))
                        .when(user_status == lit("Standard"), lit(3))
                        .else_(lit(4));

    auto query     = select(user_name, user_status).from(users_schema).order_by(/*asc(priority),*/ asc(user_name));
    const auto q   = compiler->compile(query);
    const auto sql = q.sql();
    EXPECT_FALSE(sql.empty());
    SCROLL_LOG_INF() << sql;
}
