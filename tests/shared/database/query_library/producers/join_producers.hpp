#pragma once

#include <query_expressions.hpp>

#include "../query_producer.hpp"

namespace demiplane::test {

    // Mirrors: db_join_queries_test.cpp

    template <>
    struct QueryProducer<join::InnerJoin> {
        template <db::IsSqlDialect DialectT, db::ParamMode DefaultMode>
        static db::CompiledDynamicQuery produce(const TestSchemas& s, db::QueryCompiler<DialectT, DefaultMode>& c) {
            using namespace db;
            // Mirrors: select(user_name, post_title).from(users_schema).join(posts_schema).on(...)
            auto query = select(s.users.column<"name">(), s.posts.column<"title">())
                             .from(s.users)
                             .join(s.posts)
                             .on(s.posts.column<"user_id">() == s.users.column<"id">());
            return c.compile_dynamic(query);
        }
    };

    template <>
    struct QueryProducer<join::LeftJoin> {
        template <db::IsSqlDialect DialectT, db::ParamMode DefaultMode>
        static db::CompiledDynamicQuery produce(const TestSchemas& s, db::QueryCompiler<DialectT, DefaultMode>& c) {
            using namespace db;
            // Mirrors: ...join(posts_schema, JoinType::LEFT)...
            auto query = select(s.users.column<"name">(), s.posts.column<"title">())
                             .from(s.users)
                             .join(s.posts, JoinType::LEFT)
                             .on(s.posts.column<"user_id">() == s.users.column<"id">());
            return c.compile_dynamic(query);
        }
    };

    template <>
    struct QueryProducer<join::RightJoin> {
        template <db::IsSqlDialect DialectT, db::ParamMode DefaultMode>
        static db::CompiledDynamicQuery produce(const TestSchemas& s, db::QueryCompiler<DialectT, DefaultMode>& c) {
            using namespace db;
            // Mirrors: ...join(posts_schema, JoinType::RIGHT)...
            auto query = select(s.users.column<"name">(), s.posts.column<"title">())
                             .from(s.users)
                             .join(s.posts, JoinType::RIGHT)
                             .on(s.posts.column<"user_id">() == s.users.column<"id">());
            return c.compile_dynamic(query);
        }
    };

    template <>
    struct QueryProducer<join::FullJoin> {
        template <db::IsSqlDialect DialectT, db::ParamMode DefaultMode>
        static db::CompiledDynamicQuery produce(const TestSchemas& s, db::QueryCompiler<DialectT, DefaultMode>& c) {
            using namespace db;
            // Mirrors: ...join(posts_schema, JoinType::FULL)...
            auto query = select(s.users.column<"name">(), s.posts.column<"title">())
                             .from(s.users)
                             .join(s.posts, JoinType::FULL)
                             .on(s.posts.column<"user_id">() == s.users.column<"id">());
            return c.compile_dynamic(query);
        }
    };

    template <>
    struct QueryProducer<join::CrossJoin> {
        template <db::IsSqlDialect DialectT, db::ParamMode DefaultMode>
        static db::CompiledDynamicQuery produce(const TestSchemas& s, db::QueryCompiler<DialectT, DefaultMode>& c) {
            using namespace db;
            // Mirrors: ...join(posts_schema, JoinType::CROSS).on(user_id > 0)
            auto query = select(s.users.column<"name">(), s.posts.column<"title">())
                             .from(s.users)
                             .join(s.posts, JoinType::CROSS)
                             .on(s.users.column<"id">() > 0);
            return c.compile_dynamic(query);
        }
    };

    template <>
    struct QueryProducer<join::MultipleJoins> {
        template <db::IsSqlDialect DialectT, db::ParamMode DefaultMode>
        static db::CompiledDynamicQuery produce(const TestSchemas& s, db::QueryCompiler<DialectT, DefaultMode>& c) {
            using namespace db;
            // Mirrors: simple join (placeholder for multiple joins)
            auto query = select(s.users.column<"name">(), s.posts.column<"title">())
                             .from(s.users)
                             .join(s.posts)
                             .on(s.posts.column<"user_id">() == s.users.column<"id">());
            return c.compile_dynamic(query);
        }
    };

    template <>
    struct QueryProducer<join::JoinComplexCondition> {
        template <db::IsSqlDialect DialectT, db::ParamMode DefaultMode>
        static db::CompiledDynamicQuery produce(const TestSchemas& s, db::QueryCompiler<DialectT, DefaultMode>& c) {
            using namespace db;
            // Mirrors: ...on(post_user_id == user_id && post_published == true)
            auto query =
                select(s.users.column<"name">(), s.posts.column<"title">())
                    .from(s.users)
                    .join(s.posts)
                    .on(s.posts.column<"user_id">() == s.users.column<"id">() && s.posts.column<"published">() == true);
            return c.compile_dynamic(query);
        }
    };

    template <>
    struct QueryProducer<join::JoinWithWhere> {
        template <db::IsSqlDialect DialectT, db::ParamMode DefaultMode>
        static db::CompiledDynamicQuery produce(const TestSchemas& s, db::QueryCompiler<DialectT, DefaultMode>& c) {
            using namespace db;
            // Mirrors: ...join(...).on(...).where(user_active == true)
            auto query = select(s.users.column<"name">(), s.posts.column<"title">())
                             .from(s.users)
                             .join(s.posts)
                             .on(s.posts.column<"user_id">() == s.users.column<"id">())
                             .where(s.users.column<"active">() == true);
            return c.compile_dynamic(query);
        }
    };

    template <>
    struct QueryProducer<join::JoinWithAggregates> {
        template <db::IsSqlDialect DialectT, db::ParamMode DefaultMode>
        static db::CompiledDynamicQuery produce(const TestSchemas& s, db::QueryCompiler<DialectT, DefaultMode>& c) {
            using namespace db;
            // Mirrors: select(user_name, count(post_id).as("post_count"))...group_by(user_name)
            auto query = select(s.users.column<"name">(), count(s.posts.column<"id">()).as("post_count"))
                             .from(s.users)
                             .join(s.posts, JoinType::LEFT)
                             .on(s.posts.column<"user_id">() == s.users.column<"id">())
                             .group_by(s.users.column<"name">());
            return c.compile_dynamic(query);
        }
    };

    template <>
    struct QueryProducer<join::JoinWithOrderBy> {
        template <db::IsSqlDialect DialectT, db::ParamMode DefaultMode>
        static db::CompiledDynamicQuery produce(const TestSchemas& s, db::QueryCompiler<DialectT, DefaultMode>& c) {
            using namespace db;
            // Mirrors: ...order_by(asc(user_name), desc(post_title))
            auto query = select(s.users.column<"name">(), s.posts.column<"title">())
                             .from(s.users)
                             .join(s.posts)
                             .on(s.posts.column<"user_id">() == s.users.column<"id">())
                             .order_by(asc(s.users.column<"name">()), desc(s.posts.column<"title">()));
            return c.compile_dynamic(query);
        }
    };

}  // namespace demiplane::test
