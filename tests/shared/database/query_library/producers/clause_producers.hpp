#pragma once

#include <query_expressions.hpp>

#include "../query_producer.hpp"

namespace demiplane::test {

    // Mirrors: db_clause_queries_test.cpp
    // Uses users_extended schema for department/salary fields

    template <>
    struct QueryProducer<clause::FromTable> {
        template <db::IsSqlDialect DialectT, db::ParamMode DefaultMode>
        static db::CompiledDynamicQuery produce(const TestSchemas& s, db::QueryCompiler<DialectT, DefaultMode>& c) {
            using namespace db;
            // Mirrors: FROM with Table
            auto query = select(s.users.column<"name">()).from(s.users);
            return c.compile_dynamic(query);
        }
    };

    template <>
    struct QueryProducer<clause::FromTableName> {
        template <db::IsSqlDialect DialectT, db::ParamMode DefaultMode>
        static db::CompiledDynamicQuery produce(const TestSchemas& /*s*/, db::QueryCompiler<DialectT, DefaultMode>& c) {
            using namespace db;
            // Mirrors: FROM with table name string
            auto query = select(1).from("test_table");
            return c.compile_dynamic(query);
        }
    };

    template <>
    struct QueryProducer<clause::WhereSimple> {
        template <db::IsSqlDialect DialectT, db::ParamMode DefaultMode>
        static db::CompiledDynamicQuery produce(const TestSchemas& s, db::QueryCompiler<DialectT, DefaultMode>& c) {
            using namespace db;
            // Mirrors: Simple WHERE
            auto query = select(s.users.column<"name">()).from(s.users).where(s.users.column<"active">() == true);
            return c.compile_dynamic(query);
        }
    };

    template <>
    struct QueryProducer<clause::WhereComplex> {
        template <db::IsSqlDialect DialectT, db::ParamMode DefaultMode>
        static db::CompiledDynamicQuery produce(const TestSchemas& s, db::QueryCompiler<DialectT, DefaultMode>& c) {
            using namespace db;
            // Mirrors: WHERE with AND/OR
            auto query = select(s.users.column<"name">())
                             .from(s.users_extended)
                             .where(s.users_extended.column<"age">() > 18 &&
                                    (s.users_extended.column<"active">() == true ||
                                     s.users_extended.column<"salary">() > lit(50000.0)));
            return c.compile_dynamic(query);
        }
    };

    template <>
    struct QueryProducer<clause::WhereIn> {
        template <db::IsSqlDialect DialectT, db::ParamMode DefaultMode>
        static db::CompiledDynamicQuery produce(const TestSchemas& s, db::QueryCompiler<DialectT, DefaultMode>& c) {
            using namespace db;
            // Mirrors: WHERE with IN
            auto query = select(s.users.column<"name">()).from(s.users).where(in(s.users.column<"age">(), 25, 30, 35));
            return c.compile_dynamic(query);
        }
    };

    template <>
    struct QueryProducer<clause::WhereBetween> {
        template <db::IsSqlDialect DialectT, db::ParamMode DefaultMode>
        static db::CompiledDynamicQuery produce(const TestSchemas& s, db::QueryCompiler<DialectT, DefaultMode>& c) {
            using namespace db;
            // Mirrors: WHERE with BETWEEN (salary)
            auto query = select(s.users_extended.column<"name">())
                             .from(s.users_extended)
                             .where(between(s.users_extended.column<"salary">(), lit(30000.0), lit(80000.0)));
            return c.compile_dynamic(query);
        }
    };

    template <>
    struct QueryProducer<clause::GroupBySingle> {
        template <db::IsSqlDialect DialectT, db::ParamMode DefaultMode>
        static db::CompiledDynamicQuery produce(const TestSchemas& s, db::QueryCompiler<DialectT, DefaultMode>& c) {
            using namespace db;
            // Mirrors: Single column GROUP BY
            auto query =
                select(s.users_extended.column<"department">(), count(s.users_extended.column<"id">()).as("count"))
                    .from(s.users_extended)
                    .group_by(s.users_extended.column<"department">());
            return c.compile_dynamic(query);
        }
    };

    template <>
    struct QueryProducer<clause::GroupByMultiple> {
        template <db::IsSqlDialect DialectT, db::ParamMode DefaultMode>
        static db::CompiledDynamicQuery produce(const TestSchemas& s, db::QueryCompiler<DialectT, DefaultMode>& c) {
            using namespace db;
            // Mirrors: Multiple column GROUP BY
            auto query = select(s.users_extended.column<"department">(),
                                s.users_extended.column<"active">(),
                                count(s.users_extended.column<"id">()).as("count"))
                             .from(s.users_extended)
                             .group_by(s.users_extended.column<"department">(), s.users_extended.column<"active">());
            return c.compile_dynamic(query);
        }
    };

    template <>
    struct QueryProducer<clause::GroupByWithWhere> {
        template <db::IsSqlDialect DialectT, db::ParamMode DefaultMode>
        static db::CompiledDynamicQuery produce(const TestSchemas& s, db::QueryCompiler<DialectT, DefaultMode>& c) {
            using namespace db;
            // Mirrors: GROUP BY with WHERE
            auto query = select(s.users_extended.column<"department">(),
                                avg(s.users_extended.column<"salary">()).as("avg_salary"))
                             .from(s.users_extended)
                             .where(s.users_extended.column<"active">() == true)
                             .group_by(s.users_extended.column<"department">());
            return c.compile_dynamic(query);
        }
    };

    template <>
    struct QueryProducer<clause::HavingSimple> {
        template <db::IsSqlDialect DialectT, db::ParamMode DefaultMode>
        static db::CompiledDynamicQuery produce(const TestSchemas& s, db::QueryCompiler<DialectT, DefaultMode>& c) {
            using namespace db;
            // Mirrors: HAVING with aggregate condition
            auto query =
                select(s.users_extended.column<"department">(), count(s.users_extended.column<"id">()).as("count"))
                    .from(s.users_extended)
                    .group_by(s.users_extended.column<"department">())
                    .having(count(s.users_extended.column<"id">()) > 5);
            return c.compile_dynamic(query);
        }
    };

    template <>
    struct QueryProducer<clause::HavingMultiple> {
        template <db::IsSqlDialect DialectT, db::ParamMode DefaultMode>
        static db::CompiledDynamicQuery produce(const TestSchemas& s, db::QueryCompiler<DialectT, DefaultMode>& c) {
            using namespace db;
            // Mirrors: HAVING with multiple conditions
            auto query = select(s.users_extended.column<"department">(),
                                avg(s.users_extended.column<"salary">()).as("avg_salary"),
                                count(s.users_extended.column<"id">()).as("count"))
                             .from(s.users_extended)
                             .group_by(s.users_extended.column<"department">())
                             .having(count(s.users_extended.column<"id">()) > 3 &&
                                     avg(s.users_extended.column<"salary">()) > lit(45000.0));
            return c.compile_dynamic(query);
        }
    };

    template <>
    struct QueryProducer<clause::HavingWithWhere> {
        template <db::IsSqlDialect DialectT, db::ParamMode DefaultMode>
        static db::CompiledDynamicQuery produce(const TestSchemas& s, db::QueryCompiler<DialectT, DefaultMode>& c) {
            using namespace db;
            // Mirrors: HAVING with WHERE and GROUP BY
            auto query = select(s.users_extended.column<"department">(),
                                max(s.users_extended.column<"salary">()).as("max_salary"))
                             .from(s.users_extended)
                             .where(s.users_extended.column<"active">() == true)
                             .group_by(s.users_extended.column<"department">())
                             .having(max(s.users_extended.column<"salary">()) > lit(70000.0));
            return c.compile_dynamic(query);
        }
    };

    template <>
    struct QueryProducer<clause::OrderByAsc> {
        template <db::IsSqlDialect DialectT, db::ParamMode DefaultMode>
        static db::CompiledDynamicQuery produce(const TestSchemas& s, db::QueryCompiler<DialectT, DefaultMode>& c) {
            using namespace db;
            // Mirrors: ORDER BY ASC
            auto query = select(s.users.column<"name">(), s.users.column<"age">())
                             .from(s.users)
                             .order_by(asc(s.users.column<"name">()));
            return c.compile_dynamic(query);
        }
    };

    template <>
    struct QueryProducer<clause::OrderByDesc> {
        template <db::IsSqlDialect DialectT, db::ParamMode DefaultMode>
        static db::CompiledDynamicQuery produce(const TestSchemas& s, db::QueryCompiler<DialectT, DefaultMode>& c) {
            using namespace db;
            // Mirrors: ORDER BY DESC
            auto query = select(s.users_extended.column<"name">(), s.users_extended.column<"salary">())
                             .from(s.users_extended)
                             .order_by(desc(s.users_extended.column<"salary">()));
            return c.compile_dynamic(query);
        }
    };

    template <>
    struct QueryProducer<clause::OrderByMultiple> {
        template <db::IsSqlDialect DialectT, db::ParamMode DefaultMode>
        static db::CompiledDynamicQuery produce(const TestSchemas& s, db::QueryCompiler<DialectT, DefaultMode>& c) {
            using namespace db;
            // Mirrors: Multiple column ORDER BY
            auto query = select(s.users_extended.column<"name">(),
                                s.users_extended.column<"department">(),
                                s.users_extended.column<"salary">())
                             .from(s.users_extended)
                             .order_by(asc(s.users_extended.column<"department">()),
                                       desc(s.users_extended.column<"salary">()),
                                       asc(s.users_extended.column<"name">()));
            return c.compile_dynamic(query);
        }
    };

    template <>
    struct QueryProducer<clause::LimitBasic> {
        template <db::IsSqlDialect DialectT, db::ParamMode DefaultMode>
        static db::CompiledDynamicQuery produce(const TestSchemas& s, db::QueryCompiler<DialectT, DefaultMode>& c) {
            using namespace db;
            // Mirrors: Basic LIMIT
            auto query = select(s.users.column<"name">()).from(s.users).limit(10);
            return c.compile_dynamic(query);
        }
    };

    template <>
    struct QueryProducer<clause::LimitWithOrderBy> {
        template <db::IsSqlDialect DialectT, db::ParamMode DefaultMode>
        static db::CompiledDynamicQuery produce(const TestSchemas& s, db::QueryCompiler<DialectT, DefaultMode>& c) {
            using namespace db;
            // Mirrors: LIMIT with ORDER BY
            auto query = select(s.users_extended.column<"name">(), s.users_extended.column<"salary">())
                             .from(s.users_extended)
                             .order_by(desc(s.users_extended.column<"salary">()))
                             .limit(5);
            return c.compile_dynamic(query);
        }
    };

    template <>
    struct QueryProducer<clause::LimitWithWhereOrderBy> {
        template <db::IsSqlDialect DialectT, db::ParamMode DefaultMode>
        static db::CompiledDynamicQuery produce(const TestSchemas& s, db::QueryCompiler<DialectT, DefaultMode>& c) {
            using namespace db;
            // Mirrors: LIMIT with WHERE and ORDER BY
            auto query = select(s.users.column<"name">(), s.users.column<"age">())
                             .from(s.users)
                             .where(s.users.column<"active">() == true)
                             .order_by(asc(s.users.column<"age">()))
                             .limit(20);
            return c.compile_dynamic(query);
        }
    };

    template <>
    struct QueryProducer<clause::ComplexAllClauses> {
        template <db::IsSqlDialect DialectT, db::ParamMode DefaultMode>
        static db::CompiledDynamicQuery produce(const TestSchemas& s, db::QueryCompiler<DialectT, DefaultMode>& c) {
            using namespace db;
            // Mirrors: Complex query with all clauses
            auto query =
                select(s.users_extended.column<"department">(),
                       count(s.users_extended.column<"id">()).as("employee_count"),
                       avg(s.users_extended.column<"salary">()).as("avg_salary"),
                       max(s.users_extended.column<"salary">()).as("max_salary"))
                    .from(s.users_extended)
                    .where(s.users_extended.column<"active">() == true && s.users_extended.column<"age">() >= 21)
                    .group_by(s.users_extended.column<"department">())
                    .having(count(s.users_extended.column<"id">()) >= 3 &&
                            avg(s.users_extended.column<"salary">()) > lit(40000.0))
                    .order_by(asc(s.users_extended.column<"department">()))
                    .limit(10);
            return c.compile_dynamic(query);
        }
    };

    template <>
    struct QueryProducer<clause::ClausesWithJoins> {
        template <db::IsSqlDialect DialectT, db::ParamMode DefaultMode>
        static db::CompiledDynamicQuery produce(const TestSchemas& s, db::QueryCompiler<DialectT, DefaultMode>& c) {
            using namespace db;
            // Mirrors: Clauses with JOINs
            auto query = select(s.users_extended.column<"name">(),
                                s.users_extended.column<"department">(),
                                sum(s.orders_extended.column<"amount">()).as("total_orders"))
                             .from(s.users_extended)
                             .join(s.orders_extended)
                             .on(s.orders_extended.column<"user_id">() == s.users_extended.column<"id">())
                             .where(s.users_extended.column<"active">() == true &&
                                    s.orders_extended.column<"status">() == "completed")
                             .group_by(s.users_extended.column<"id">(),
                                       s.users_extended.column<"name">(),
                                       s.users_extended.column<"department">())
                             .having(sum(s.orders_extended.column<"amount">()) > lit(1000.0))
                             .order_by(desc(s.users_extended.column<"name">()))
                             .limit(5);
            return c.compile_dynamic(query);
        }
    };

}  // namespace demiplane::test
