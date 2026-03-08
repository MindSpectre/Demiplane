#pragma once

#include <query_expressions.hpp>

#include "../query_producer.hpp"

namespace demiplane::test {

    // Mirrors: db_condition_queries_test.cpp

    template <>
    struct QueryProducer<condition::BinaryEqual> {
        template <db::IsSqlDialect DialectT, db::ParamMode DefaultMode>
        static db::CompiledDynamicQuery produce(const TestSchemas& s, db::QueryCompiler<DialectT, DefaultMode>& c) {
            using namespace db;
            // Mirrors: user_age == 25
            auto query = select(s.users.column<"name">()).from(s.users).where(s.users.column<"age">() == 25);
            return c.compile_dynamic(query);
        }
    };

    template <>
    struct QueryProducer<condition::BinaryNotEqual> {
        template <db::IsSqlDialect DialectT, db::ParamMode DefaultMode>
        static db::CompiledDynamicQuery produce(const TestSchemas& s, db::QueryCompiler<DialectT, DefaultMode>& c) {
            using namespace db;
            // Mirrors: user_age != 25
            auto query = select(s.users.column<"name">()).from(s.users).where(s.users.column<"age">() != 25);
            return c.compile_dynamic(query);
        }
    };

    template <>
    struct QueryProducer<condition::BinaryGreater> {
        template <db::IsSqlDialect DialectT, db::ParamMode DefaultMode>
        static db::CompiledDynamicQuery produce(const TestSchemas& s, db::QueryCompiler<DialectT, DefaultMode>& c) {
            using namespace db;
            // Mirrors: user_age > 18
            auto query = select(s.users.column<"name">()).from(s.users).where(s.users.column<"age">() > 18);
            return c.compile_dynamic(query);
        }
    };

    template <>
    struct QueryProducer<condition::BinaryGreaterEqual> {
        template <db::IsSqlDialect DialectT, db::ParamMode DefaultMode>
        static db::CompiledDynamicQuery produce(const TestSchemas& s, db::QueryCompiler<DialectT, DefaultMode>& c) {
            using namespace db;
            // Mirrors: user_age >= 18
            auto query = select(s.users.column<"name">()).from(s.users).where(s.users.column<"age">() >= 18);
            return c.compile_dynamic(query);
        }
    };

    template <>
    struct QueryProducer<condition::BinaryLess> {
        template <db::IsSqlDialect DialectT, db::ParamMode DefaultMode>
        static db::CompiledDynamicQuery produce(const TestSchemas& s, db::QueryCompiler<DialectT, DefaultMode>& c) {
            using namespace db;
            // Mirrors: user_age < 65
            auto query = select(s.users.column<"name">()).from(s.users).where(s.users.column<"age">() < 65);
            return c.compile_dynamic(query);
        }
    };

    template <>
    struct QueryProducer<condition::BinaryLessEqual> {
        template <db::IsSqlDialect DialectT, db::ParamMode DefaultMode>
        static db::CompiledDynamicQuery produce(const TestSchemas& s, db::QueryCompiler<DialectT, DefaultMode>& c) {
            using namespace db;
            // Mirrors: user_age <= 65
            auto query = select(s.users.column<"name">()).from(s.users).where(s.users.column<"age">() <= 65);
            return c.compile_dynamic(query);
        }
    };

    template <>
    struct QueryProducer<condition::LogicalAnd> {
        template <db::IsSqlDialect DialectT, db::ParamMode DefaultMode>
        static db::CompiledDynamicQuery produce(const TestSchemas& s, db::QueryCompiler<DialectT, DefaultMode>& c) {
            using namespace db;
            // Mirrors: user_age > 18 && user_active == true
            auto query = select(s.users.column<"name">())
                             .from(s.users)
                             .where(s.users.column<"age">() > 18 && s.users.column<"active">() == true);
            return c.compile_dynamic(query);
        }
    };

    template <>
    struct QueryProducer<condition::LogicalOr> {
        template <db::IsSqlDialect DialectT, db::ParamMode DefaultMode>
        static db::CompiledDynamicQuery produce(const TestSchemas& s, db::QueryCompiler<DialectT, DefaultMode>& c) {
            using namespace db;
            // Mirrors: user_age < 18 || user_age > 65
            auto query = select(s.users.column<"name">())
                             .from(s.users)
                             .where(s.users.column<"age">() < 18 || s.users.column<"age">() > 65);
            return c.compile_dynamic(query);
        }
    };

    template <>
    struct QueryProducer<condition::UnaryCondition> {
        template <db::IsSqlDialect DialectT, db::ParamMode DefaultMode>
        static db::CompiledDynamicQuery produce(const TestSchemas& s, db::QueryCompiler<DialectT, DefaultMode>& c) {
            using namespace db;
            // Mirrors: NOT condition via user_active == false
            auto query = select(s.users.column<"name">()).from(s.users).where(s.users.column<"active">() == false);
            return c.compile_dynamic(query);
        }
    };

    template <>
    struct QueryProducer<condition::StringComparison> {
        template <db::IsSqlDialect DialectT, db::ParamMode DefaultMode>
        static db::CompiledDynamicQuery produce(const TestSchemas& s, db::QueryCompiler<DialectT, DefaultMode>& c) {
            using namespace db;
            // Mirrors: user_name == "john"
            auto query = select(s.users.column<"name">()).from(s.users).where(s.users.column<"name">() == "john");
            return c.compile_dynamic(query);
        }
    };

    template <>
    struct QueryProducer<condition::Between> {
        template <db::IsSqlDialect DialectT, db::ParamMode DefaultMode>
        static db::CompiledDynamicQuery produce(const TestSchemas& s, db::QueryCompiler<DialectT, DefaultMode>& c) {
            using namespace db;
            // Mirrors: between(user_age, 18, 65)
            auto query = select(s.users.column<"name">()).from(s.users).where(between(s.users.column<"age">(), 18, 65));
            return c.compile_dynamic(query);
        }
    };

    template <>
    struct QueryProducer<condition::InList> {
        template <db::IsSqlDialect DialectT, db::ParamMode DefaultMode>
        static db::CompiledDynamicQuery produce(const TestSchemas& s, db::QueryCompiler<DialectT, DefaultMode>& c) {
            using namespace db;
            // Mirrors: in(user_age, 18, 25, 30)
            auto query = select(s.users.column<"name">()).from(s.users).where(in(s.users.column<"age">(), 18, 25, 30));
            return c.compile_dynamic(query);
        }
    };

    template <>
    struct QueryProducer<condition::ExistsCondition> {
        template <db::IsSqlDialect DialectT, db::ParamMode DefaultMode>
        static db::CompiledDynamicQuery produce(const TestSchemas& s, db::QueryCompiler<DialectT, DefaultMode>& c) {
            using namespace db;
            // Mirrors: EXISTS condition
            auto subq  = select(lit(1)).from(s.posts).where(s.posts.column<"user_id">() == s.users.column<"id">() &&
                                                           s.posts.column<"published">() == lit(true));
            auto query = select(s.users.column<"name">()).from(s.users).where(exists(subq));
            return c.compile_dynamic(query);
        }
    };

    template <>
    struct QueryProducer<condition::SubqueryCondition> {
        template <db::IsSqlDialect DialectT, db::ParamMode DefaultMode>
        static db::CompiledDynamicQuery produce(const TestSchemas& s, db::QueryCompiler<DialectT, DefaultMode>& c) {
            using namespace db;
            // Mirrors: subquery in condition
            auto active_users = select(s.users.column<"id">()).from(s.users).where(s.users.column<"active">() == true);
            auto query        = select(s.posts.column<"title">())
                             .from(s.posts)
                             .where(in(s.posts.column<"user_id">(), subquery(active_users)));
            return c.compile_dynamic(query);
        }
    };

    template <>
    struct QueryProducer<condition::ComplexNested> {
        template <db::IsSqlDialect DialectT, db::ParamMode DefaultMode>
        static db::CompiledDynamicQuery produce(const TestSchemas& s, db::QueryCompiler<DialectT, DefaultMode>& c) {
            using namespace db;
            // Mirrors: (user_age > 18 && user_age < 65) || (user_active == true && user_age >= 65)
            auto query = select(s.users.column<"name">())
                             .from(s.users)
                             .where((s.users.column<"age">() > 18 && s.users.column<"age">() < 65) ||
                                    (s.users.column<"active">() == true && s.users.column<"age">() >= 65));
            return c.compile_dynamic(query);
        }
    };

}  // namespace demiplane::test
