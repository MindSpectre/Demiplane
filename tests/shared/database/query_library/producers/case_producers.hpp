#pragma once

#include <query_expressions.hpp>

#include "../query_producer.hpp"

namespace demiplane::test {

    // CASE expression query producers

    template <>
    struct QueryProducer<case_expr::SimpleCaseWhen> {
        template <db::IsSqlDialect DialectT, db::ParamMode DefaultMode>
        static db::CompiledDynamicQuery produce(const TestSchemas& s, db::QueryCompiler<DialectT, DefaultMode>& c) {
            using namespace db;
            // Simple CASE WHEN: CASE WHEN active = true THEN 'Active' END
            auto case_active = case_when(s.users.column<"active">() == true, lit("Active"));
            auto query       = select(s.users.column<"name">(), case_active.as("status")).from(s.users);
            return c.compile_dynamic(query);
        }
    };

    template <>
    struct QueryProducer<case_expr::CaseWithElse> {
        template <db::IsSqlDialect DialectT, db::ParamMode DefaultMode>
        static db::CompiledDynamicQuery produce(const TestSchemas& s, db::QueryCompiler<DialectT, DefaultMode>& c) {
            using namespace db;
            // CASE with ELSE: CASE WHEN active = true THEN 'Active' ELSE 'Inactive' END
            auto case_status = case_when(s.users.column<"active">() == true, lit("Active")).else_(lit("Inactive"));
            auto query       = select(s.users.column<"name">(), case_status.as("status")).from(s.users);
            return c.compile_dynamic(query);
        }
    };

    template <>
    struct QueryProducer<case_expr::CaseMultipleWhen> {
        template <db::IsSqlDialect DialectT, db::ParamMode DefaultMode>
        static db::CompiledDynamicQuery produce(const TestSchemas& s, db::QueryCompiler<DialectT, DefaultMode>& c) {
            using namespace db;
            // CASE with multiple WHEN clauses
            auto age_category = case_when(s.users.column<"age">() < lit(25), lit("Young"))
                                    .when(s.users.column<"age">() < lit(40), lit("Adult"))
                                    .when(s.users.column<"age">() < lit(60), lit("Middle-aged"))
                                    .else_(lit("Senior"));
            auto query =
                select(s.users.column<"name">(), s.users.column<"age">(), age_category.as("age_group")).from(s.users);
            return c.compile_dynamic(query);
        }
    };

    template <>
    struct QueryProducer<case_expr::CaseInSelect> {
        template <db::IsSqlDialect DialectT, db::ParamMode DefaultMode>
        static db::CompiledDynamicQuery produce(const TestSchemas& s, db::QueryCompiler<DialectT, DefaultMode>& c) {
            using namespace db;
            // CASE used in SELECT to categorize data
            auto order_size = case_when(s.orders.column<"amount">() < lit(100.0), lit("Small"))
                                  .when(s.orders.column<"amount">() < lit(500.0), lit("Medium"))
                                  .else_(lit("Large"));
            auto query = select(s.orders.column<"id">(), s.orders.column<"amount">(), order_size.as("order_size"))
                             .from(s.orders)
                             .where(s.orders.column<"completed">() == true);
            return c.compile_dynamic(query);
        }
    };

    template <>
    struct QueryProducer<case_expr::CaseWithComparison> {
        template <db::IsSqlDialect DialectT, db::ParamMode DefaultMode>
        static db::CompiledDynamicQuery produce(const TestSchemas& s, db::QueryCompiler<DialectT, DefaultMode>& c) {
            using namespace db;
            // CASE with various comparison operators
            auto priority = case_when(s.orders.column<"amount">() > lit(1000.0), lit(1))
                                .when(s.orders.column<"amount">() > lit(500.0), lit(2))
                                .when(s.orders.column<"amount">() > lit(100.0), lit(3))
                                .else_(lit(4));
            auto query =
                select(s.orders.column<"id">(), s.orders.column<"amount">(), priority.as("priority")).from(s.orders);
            return c.compile_dynamic(query);
        }
    };

    template <>
    struct QueryProducer<case_expr::CaseNested> {
        template <db::IsSqlDialect DialectT, db::ParamMode DefaultMode>
        static db::CompiledDynamicQuery produce(const TestSchemas& s, db::QueryCompiler<DialectT, DefaultMode>& c) {
            using namespace db;
            // Nested CASE expression in WHERE clause condition
            auto high_value = case_when(s.users.column<"active">() == true,
                                        case_when(s.users.column<"age">() > lit(30), lit("VIP")).else_(lit("Regular")))
                                  .else_(lit("Inactive"));
            auto query = select(s.users.column<"name">(), high_value.as("customer_type")).from(s.users);
            return c.compile_dynamic(query);
        }
    };

}  // namespace demiplane::test
