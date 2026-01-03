#pragma once

#include "../query_producer.hpp"

#include <query_expressions.hpp>

namespace demiplane::test {

// CASE expression query producers

template <>
struct QueryProducer<case_expr::SimpleCaseWhen> {
    static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
        using namespace db;
        // Simple CASE WHEN: CASE WHEN active = true THEN 'Active' END
        auto case_active = case_when(s.users().active == true, lit("Active"));
        auto query = select(s.users().name, case_active.as("status")).from(s.users().table);
        return c.compile(query);
    }
};

template <>
struct QueryProducer<case_expr::CaseWithElse> {
    static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
        using namespace db;
        // CASE with ELSE: CASE WHEN active = true THEN 'Active' ELSE 'Inactive' END
        auto case_status = case_when(s.users().active == true, lit("Active"))
                               .else_(lit("Inactive"));
        auto query = select(s.users().name, case_status.as("status")).from(s.users().table);
        return c.compile(query);
    }
};

template <>
struct QueryProducer<case_expr::CaseMultipleWhen> {
    static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
        using namespace db;
        // CASE with multiple WHEN clauses
        auto age_category = case_when(s.users().age < lit(25), lit("Young"))
                                .when(s.users().age < lit(40), lit("Adult"))
                                .when(s.users().age < lit(60), lit("Middle-aged"))
                                .else_(lit("Senior"));
        auto query = select(s.users().name, s.users().age, age_category.as("age_group"))
                         .from(s.users().table);
        return c.compile(query);
    }
};

template <>
struct QueryProducer<case_expr::CaseInSelect> {
    static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
        using namespace db;
        // CASE used in SELECT to categorize data
        auto order_size = case_when(s.orders().amount < lit(100.0), lit("Small"))
                              .when(s.orders().amount < lit(500.0), lit("Medium"))
                              .else_(lit("Large"));
        auto query = select(s.orders().id, s.orders().amount, order_size.as("order_size"))
                         .from(s.orders().table)
                         .where(s.orders().completed == true);
        return c.compile(query);
    }
};

template <>
struct QueryProducer<case_expr::CaseWithComparison> {
    static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
        using namespace db;
        // CASE with various comparison operators
        auto priority = case_when(s.orders().amount > lit(1000.0), lit(1))
                            .when(s.orders().amount > lit(500.0), lit(2))
                            .when(s.orders().amount > lit(100.0), lit(3))
                            .else_(lit(4));
        auto query = select(s.orders().id, s.orders().amount, priority.as("priority"))
                         .from(s.orders().table);
        return c.compile(query);
    }
};

template <>
struct QueryProducer<case_expr::CaseNested> {
    static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
        using namespace db;
        // Nested CASE expression in WHERE clause condition
        auto high_value = case_when(s.users().active == true,
                                    case_when(s.users().age > lit(30), lit("VIP"))
                                        .else_(lit("Regular")))
                              .else_(lit("Inactive"));
        auto query = select(s.users().name, high_value.as("customer_type"))
                         .from(s.users().table);
        return c.compile(query);
    }
};

}  // namespace demiplane::test
