#pragma once

#include <db_record.hpp>
#include <query_expressions.hpp>

#include "../query_producer.hpp"

namespace demiplane::test {

    // Mirrors: db_select_queries_test.cpp

    template <>
    struct QueryProducer<sel::BasicSelect> {
        template <db::IsSqlDialect DialectT, db::ParamMode DefaultMode>
        static db::CompiledDynamicQuery produce(const TestSchemas& s, db::QueryCompiler<DialectT, DefaultMode>& c) {
            using namespace db;
            // Mirrors: select(user_id, user_name)
            auto query = select(s.users.column<"id">(), s.users.column<"name">()).from(s.users);
            return c.compile_dynamic(query);
        }
    };

    template <>
    struct QueryProducer<sel::SelectAllColumns> {
        template <db::IsSqlDialect DialectT, db::ParamMode DefaultMode>
        static db::CompiledDynamicQuery produce(const TestSchemas& s, db::QueryCompiler<DialectT, DefaultMode>& c) {
            using namespace db;
            // Mirrors: select(all("users"))
            auto query = select(all("users")).from(s.users);
            return c.compile_dynamic(query);
        }
    };

    template <>
    struct QueryProducer<sel::SelectDistinct> {
        template <db::IsSqlDialect DialectT, db::ParamMode DefaultMode>
        static db::CompiledDynamicQuery produce(const TestSchemas& s, db::QueryCompiler<DialectT, DefaultMode>& c) {
            using namespace db;
            // Mirrors: select_distinct(user_name, user_age)
            auto query = select_distinct(s.users.column<"name">(), s.users.column<"age">()).from(s.users);
            return c.compile_dynamic(query);
        }
    };

    template <>
    struct QueryProducer<sel::SelectMixedTypes> {
        template <db::IsSqlDialect DialectT, db::ParamMode DefaultMode>
        static db::CompiledDynamicQuery produce(const TestSchemas& s, db::QueryCompiler<DialectT, DefaultMode>& c) {
            using namespace db;
            // Mirrors: select(user_name, "constant", count(user_id).as("total")) with GROUP BY
            auto query = select(s.users.column<"name">(), "constant", count(s.users.column<"id">()).as("total"))
                             .from(s.users)
                             .group_by(s.users.column<"name">());
            return c.compile_dynamic(query);
        }
    };

    template <>
    struct QueryProducer<sel::SelectFromRecord> {
        template <db::IsSqlDialect DialectT, db::ParamMode DefaultMode>
        static db::CompiledDynamicQuery produce(const TestSchemas& s, db::QueryCompiler<DialectT, DefaultMode>& c) {
            using namespace db;
            // Mirrors: select(user_name).from(test_record)
            Record test_record(s.users_dynamic);
            test_record["id"].set(1);
            test_record["name"].set(std::string("test"));
            auto query = select(s.users.column<"name">()).from(test_record);
            return c.compile_dynamic(query);
        }
    };

    template <>
    struct QueryProducer<sel::SelectFromTableName> {
        template <db::IsSqlDialect DialectT, db::ParamMode DefaultMode>
        static db::CompiledDynamicQuery produce(const TestSchemas& /*s*/, db::QueryCompiler<DialectT, DefaultMode>& c) {
            using namespace db;
            // Mirrors: select(1).from("test_table")
            auto query = select(1).from("test_table");
            return c.compile_dynamic(query);
        }
    };

    template <>
    struct QueryProducer<sel::SelectWithWhere> {
        template <db::IsSqlDialect DialectT, db::ParamMode DefaultMode>
        static db::CompiledDynamicQuery produce(const TestSchemas& s, db::QueryCompiler<DialectT, DefaultMode>& c) {
            using namespace db;
            // Mirrors: select(user_name).from(users_schema).where(user_age > 18)
            auto query = select(s.users.column<"name">()).from(s.users).where(s.users.column<"age">() > 18);
            return c.compile_dynamic(query);
        }
    };

    template <>
    struct QueryProducer<sel::SelectWithJoin> {
        template <db::IsSqlDialect DialectT, db::ParamMode DefaultMode>
        static db::CompiledDynamicQuery produce(const TestSchemas& s, db::QueryCompiler<DialectT, DefaultMode>& c) {
            using namespace db;
            // Mirrors: select(user_name, post_title).from(users).join(posts).on(...)
            auto query = select(s.users.column<"name">(), s.posts.column<"title">())
                             .from(s.users)
                             .join(s.posts)
                             .on(s.posts.column<"user_id">() == s.users.column<"id">());
            return c.compile_dynamic(query);
        }
    };

    template <>
    struct QueryProducer<sel::SelectWithGroupBy> {
        template <db::IsSqlDialect DialectT, db::ParamMode DefaultMode>
        static db::CompiledDynamicQuery produce(const TestSchemas& s, db::QueryCompiler<DialectT, DefaultMode>& c) {
            using namespace db;
            // Mirrors: select(user_active, count(user_id).as("user_count")).from(users_schema).group_by(user_active)
            auto query = select(s.users.column<"active">(), count(s.users.column<"id">()).as("user_count"))
                             .from(s.users)
                             .group_by(s.users.column<"active">());
            return c.compile_dynamic(query);
        }
    };

    template <>
    struct QueryProducer<sel::SelectWithHaving> {
        template <db::IsSqlDialect DialectT, db::ParamMode DefaultMode>
        static db::CompiledDynamicQuery produce(const TestSchemas& s, db::QueryCompiler<DialectT, DefaultMode>& c) {
            using namespace db;
            // Mirrors: select(...).from(...).group_by(...).having(count(user_id) > 5)
            auto query = select(s.users.column<"active">(), count(s.users.column<"id">()).as("user_count"))
                             .from(s.users)
                             .group_by(s.users.column<"active">())
                             .having(count(s.users.column<"id">()) > 5);
            return c.compile_dynamic(query);
        }
    };

    template <>
    struct QueryProducer<sel::SelectWithOrderBy> {
        template <db::IsSqlDialect DialectT, db::ParamMode DefaultMode>
        static db::CompiledDynamicQuery produce(const TestSchemas& s, db::QueryCompiler<DialectT, DefaultMode>& c) {
            using namespace db;
            // Mirrors: select(user_name).from(users_schema).order_by(asc(user_name))
            auto query = select(s.users.column<"name">()).from(s.users).order_by(asc(s.users.column<"name">()));
            return c.compile_dynamic(query);
        }
    };

    template <>
    struct QueryProducer<sel::SelectWithLimit> {
        template <db::IsSqlDialect DialectT, db::ParamMode DefaultMode>
        static db::CompiledDynamicQuery produce(const TestSchemas& s, db::QueryCompiler<DialectT, DefaultMode>& c) {
            using namespace db;
            // Mirrors: select(user_name).from(users_schema).limit(10)
            auto query = select(s.users.column<"name">()).from(s.users).limit(10);
            return c.compile_dynamic(query);
        }
    };

}  // namespace demiplane::test
