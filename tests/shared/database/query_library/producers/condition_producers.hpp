#pragma once

#include <query_expressions.hpp>

#include "../query_producer.hpp"

namespace demiplane::test {

    // Mirrors: db_condition_queries_test.cpp

    template <>
    struct QueryProducer<condition::BinaryEqual> {
        template <db::IsSqlDialect DialectTp>
        static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler<DialectTp>& c) {
            using namespace db;
            // Mirrors: user_age == 25
            auto query = select(s.users().name).from(s.users().table).where(s.users().age == 25);
            return c.compile(query);
        }
    };

    template <>
    struct QueryProducer<condition::BinaryNotEqual> {
        template <db::IsSqlDialect DialectTp>
        static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler<DialectTp>& c) {
            using namespace db;
            // Mirrors: user_age != 25
            auto query = select(s.users().name).from(s.users().table).where(s.users().age != 25);
            return c.compile(query);
        }
    };

    template <>
    struct QueryProducer<condition::BinaryGreater> {
        template <db::IsSqlDialect DialectTp>
        static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler<DialectTp>& c) {
            using namespace db;
            // Mirrors: user_age > 18
            auto query = select(s.users().name).from(s.users().table).where(s.users().age > 18);
            return c.compile(query);
        }
    };

    template <>
    struct QueryProducer<condition::BinaryGreaterEqual> {
        template <db::IsSqlDialect DialectTp>
        static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler<DialectTp>& c) {
            using namespace db;
            // Mirrors: user_age >= 18
            auto query = select(s.users().name).from(s.users().table).where(s.users().age >= 18);
            return c.compile(query);
        }
    };

    template <>
    struct QueryProducer<condition::BinaryLess> {
        template <db::IsSqlDialect DialectTp>
        static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler<DialectTp>& c) {
            using namespace db;
            // Mirrors: user_age < 65
            auto query = select(s.users().name).from(s.users().table).where(s.users().age < 65);
            return c.compile(query);
        }
    };

    template <>
    struct QueryProducer<condition::BinaryLessEqual> {
        template <db::IsSqlDialect DialectTp>
        static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler<DialectTp>& c) {
            using namespace db;
            // Mirrors: user_age <= 65
            auto query = select(s.users().name).from(s.users().table).where(s.users().age <= 65);
            return c.compile(query);
        }
    };

    template <>
    struct QueryProducer<condition::LogicalAnd> {
        template <db::IsSqlDialect DialectTp>
        static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler<DialectTp>& c) {
            using namespace db;
            // Mirrors: user_age > 18 && user_active == true
            auto query =
                select(s.users().name).from(s.users().table).where(s.users().age > 18 && s.users().active == true);
            return c.compile(query);
        }
    };

    template <>
    struct QueryProducer<condition::LogicalOr> {
        template <db::IsSqlDialect DialectTp>
        static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler<DialectTp>& c) {
            using namespace db;
            // Mirrors: user_age < 18 || user_age > 65
            auto query = select(s.users().name).from(s.users().table).where(s.users().age < 18 || s.users().age > 65);
            return c.compile(query);
        }
    };

    template <>
    struct QueryProducer<condition::UnaryCondition> {
        template <db::IsSqlDialect DialectTp>
        static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler<DialectTp>& c) {
            using namespace db;
            // Mirrors: NOT condition via user_active == false
            auto query = select(s.users().name).from(s.users().table).where(s.users().active == false);
            return c.compile(query);
        }
    };

    template <>
    struct QueryProducer<condition::StringComparison> {
        template <db::IsSqlDialect DialectTp>
        static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler<DialectTp>& c) {
            using namespace db;
            // Mirrors: user_name == "john"
            auto query = select(s.users().name).from(s.users().table).where(s.users().name == "john");
            return c.compile(query);
        }
    };

    template <>
    struct QueryProducer<condition::Between> {
        template <db::IsSqlDialect DialectTp>
        static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler<DialectTp>& c) {
            using namespace db;
            // Mirrors: between(user_age, 18, 65)
            auto query = select(s.users().name).from(s.users().table).where(between(s.users().age, 18, 65));
            return c.compile(query);
        }
    };

    template <>
    struct QueryProducer<condition::InList> {
        template <db::IsSqlDialect DialectTp>
        static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler<DialectTp>& c) {
            using namespace db;
            // Mirrors: in(user_age, 18, 25, 30)
            auto query = select(s.users().name).from(s.users().table).where(in(s.users().age, 18, 25, 30));
            return c.compile(query);
        }
    };

    template <>
    struct QueryProducer<condition::ExistsCondition> {
        template <db::IsSqlDialect DialectTp>
        static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler<DialectTp>& c) {
            using namespace db;
            // Mirrors: EXISTS condition
            auto subq = select(lit(1))
                            .from(s.posts().table)
                            .where(s.posts().user_id == s.users().id && s.posts().published == lit(true));
            auto query = select(s.users().name).from(s.users().table).where(exists(subq));
            return c.compile(query);
        }
    };

    template <>
    struct QueryProducer<condition::SubqueryCondition> {
        template <db::IsSqlDialect DialectTp>
        static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler<DialectTp>& c) {
            using namespace db;
            // Mirrors: subquery in condition
            auto active_users = select(s.users().id).from(s.users().table).where(s.users().active == true);
            auto query =
                select(s.posts().title).from(s.posts().table).where(in(s.posts().user_id, subquery(active_users)));
            return c.compile(query);
        }
    };

    template <>
    struct QueryProducer<condition::ComplexNested> {
        template <db::IsSqlDialect DialectTp>
        static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler<DialectTp>& c) {
            using namespace db;
            // Mirrors: (user_age > 18 && user_age < 65) || (user_active == true && user_age >= 65)
            auto query = select(s.users().name)
                             .from(s.users().table)
                             .where((s.users().age > 18 && s.users().age < 65) ||
                                    (s.users().active == true && s.users().age >= 65));
            return c.compile(query);
        }
    };

}  // namespace demiplane::test
