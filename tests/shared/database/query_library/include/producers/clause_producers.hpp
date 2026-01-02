#pragma once

#include "../query_producer.hpp"

#include <query_expressions.hpp>

namespace demiplane::test {

// Mirrors: db_clause_queries_test.cpp
// Uses users_extended schema for department/salary fields

template <>
struct QueryProducer<clause::FromTable> {
    static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
        using namespace db;
        // Mirrors: FROM with Table
        auto query = select(s.users().name).from(s.users().table);
        return c.compile(query);
    }
};

template <>
struct QueryProducer<clause::FromTableName> {
    static db::CompiledQuery produce(const TestSchemas& /*s*/, db::QueryCompiler& c) {
        using namespace db;
        // Mirrors: FROM with table name string
        auto query = select(1).from("test_table");
        return c.compile(query);
    }
};

template <>
struct QueryProducer<clause::WhereSimple> {
    static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
        using namespace db;
        // Mirrors: Simple WHERE
        auto query = select(s.users().name).from(s.users().table).where(s.users().active == true);
        return c.compile(query);
    }
};

template <>
struct QueryProducer<clause::WhereComplex> {
    static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
        using namespace db;
        // Mirrors: WHERE with AND/OR
        auto query = select(s.users().name)
                         .from(s.users_extended().table)
                         .where(s.users_extended().age > 18 &&
                                (s.users_extended().active == true || s.users_extended().salary > lit(50000.0)));
        return c.compile(query);
    }
};

template <>
struct QueryProducer<clause::WhereIn> {
    static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
        using namespace db;
        // Mirrors: WHERE with IN
        auto query = select(s.users().name).from(s.users().table).where(in(s.users().age, 25, 30, 35));
        return c.compile(query);
    }
};

template <>
struct QueryProducer<clause::WhereBetween> {
    static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
        using namespace db;
        // Mirrors: WHERE with BETWEEN (salary)
        auto query = select(s.users_extended().name)
                         .from(s.users_extended().table)
                         .where(between(s.users_extended().salary, lit(30000.0), lit(80000.0)));
        return c.compile(query);
    }
};

template <>
struct QueryProducer<clause::GroupBySingle> {
    static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
        using namespace db;
        // Mirrors: Single column GROUP BY
        auto query = select(s.users_extended().department, count(s.users_extended().id).as("count"))
                         .from(s.users_extended().table)
                         .group_by(s.users_extended().department);
        return c.compile(query);
    }
};

template <>
struct QueryProducer<clause::GroupByMultiple> {
    static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
        using namespace db;
        // Mirrors: Multiple column GROUP BY
        auto query = select(s.users_extended().department, s.users_extended().active, count(s.users_extended().id).as("count"))
                         .from(s.users_extended().table)
                         .group_by(s.users_extended().department, s.users_extended().active);
        return c.compile(query);
    }
};

template <>
struct QueryProducer<clause::GroupByWithWhere> {
    static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
        using namespace db;
        // Mirrors: GROUP BY with WHERE
        auto query = select(s.users_extended().department, avg(s.users_extended().salary).as("avg_salary"))
                         .from(s.users_extended().table)
                         .where(s.users_extended().active == true)
                         .group_by(s.users_extended().department);
        return c.compile(query);
    }
};

template <>
struct QueryProducer<clause::HavingSimple> {
    static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
        using namespace db;
        // Mirrors: HAVING with aggregate condition
        auto query = select(s.users_extended().department, count(s.users_extended().id).as("count"))
                         .from(s.users_extended().table)
                         .group_by(s.users_extended().department)
                         .having(count(s.users_extended().id) > 5);
        return c.compile(query);
    }
};

template <>
struct QueryProducer<clause::HavingMultiple> {
    static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
        using namespace db;
        // Mirrors: HAVING with multiple conditions
        auto query = select(s.users_extended().department,
                            avg(s.users_extended().salary).as("avg_salary"),
                            count(s.users_extended().id).as("count"))
                         .from(s.users_extended().table)
                         .group_by(s.users_extended().department)
                         .having(count(s.users_extended().id) > 3 && avg(s.users_extended().salary) > lit(45000.0));
        return c.compile(query);
    }
};

template <>
struct QueryProducer<clause::HavingWithWhere> {
    static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
        using namespace db;
        // Mirrors: HAVING with WHERE and GROUP BY
        auto query = select(s.users_extended().department, max(s.users_extended().salary).as("max_salary"))
                         .from(s.users_extended().table)
                         .where(s.users_extended().active == true)
                         .group_by(s.users_extended().department)
                         .having(max(s.users_extended().salary) > lit(70000.0));
        return c.compile(query);
    }
};

template <>
struct QueryProducer<clause::OrderByAsc> {
    static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
        using namespace db;
        // Mirrors: ORDER BY ASC
        auto query = select(s.users().name, s.users().age).from(s.users().table).order_by(asc(s.users().name));
        return c.compile(query);
    }
};

template <>
struct QueryProducer<clause::OrderByDesc> {
    static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
        using namespace db;
        // Mirrors: ORDER BY DESC
        auto query = select(s.users_extended().name, s.users_extended().salary)
                         .from(s.users_extended().table)
                         .order_by(desc(s.users_extended().salary));
        return c.compile(query);
    }
};

template <>
struct QueryProducer<clause::OrderByMultiple> {
    static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
        using namespace db;
        // Mirrors: Multiple column ORDER BY
        auto query = select(s.users_extended().name, s.users_extended().department, s.users_extended().salary)
                         .from(s.users_extended().table)
                         .order_by(asc(s.users_extended().department),
                                   desc(s.users_extended().salary),
                                   asc(s.users_extended().name));
        return c.compile(query);
    }
};

template <>
struct QueryProducer<clause::LimitBasic> {
    static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
        using namespace db;
        // Mirrors: Basic LIMIT
        auto query = select(s.users().name).from(s.users().table).limit(10);
        return c.compile(query);
    }
};

template <>
struct QueryProducer<clause::LimitWithOrderBy> {
    static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
        using namespace db;
        // Mirrors: LIMIT with ORDER BY
        auto query = select(s.users_extended().name, s.users_extended().salary)
                         .from(s.users_extended().table)
                         .order_by(desc(s.users_extended().salary))
                         .limit(5);
        return c.compile(query);
    }
};

template <>
struct QueryProducer<clause::LimitWithWhereOrderBy> {
    static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
        using namespace db;
        // Mirrors: LIMIT with WHERE and ORDER BY
        auto query = select(s.users().name, s.users().age)
                         .from(s.users().table)
                         .where(s.users().active == true)
                         .order_by(asc(s.users().age))
                         .limit(20);
        return c.compile(query);
    }
};

template <>
struct QueryProducer<clause::ComplexAllClauses> {
    static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
        using namespace db;
        // Mirrors: Complex query with all clauses
        // ORDER BY uses columns from GROUP BY
        auto query = select(s.users_extended().department,
                            count(s.users_extended().id).as("employee_count"),
                            avg(s.users_extended().salary).as("avg_salary"),
                            max(s.users_extended().salary).as("max_salary"))
                         .from(s.users_extended().table)
                         .where(s.users_extended().active == true && s.users_extended().age >= 21)
                         .group_by(s.users_extended().department)
                         .having(count(s.users_extended().id) >= 3 && avg(s.users_extended().salary) > lit(40000.0))
                         .order_by(asc(s.users_extended().department))
                         .limit(10);
        return c.compile(query);
    }
};

template <>
struct QueryProducer<clause::ClausesWithJoins> {
    static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
        using namespace db;
        // Mirrors: Clauses with JOINs
        // ORDER BY uses columns from GROUP BY
        auto query = select(s.users_extended().name, s.users_extended().department, sum(s.orders_extended().amount).as("total_orders"))
                         .from(s.users_extended().table)
                         .join(s.orders_extended().table->table_name())
                         .on(s.orders_extended().user_id == s.users_extended().id)
                         .where(s.users_extended().active == true && s.orders_extended().status == "completed")
                         .group_by(s.users_extended().id, s.users_extended().name, s.users_extended().department)
                         .having(sum(s.orders_extended().amount) > lit(1000.0))
                         .order_by(desc(s.users_extended().name))
                         .limit(5);
        return c.compile(query);
    }
};

}  // namespace demiplane::test
