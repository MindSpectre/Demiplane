#pragma once

#include <query_expressions.hpp>

#include "../query_producer.hpp"

namespace demiplane::test {

    // Mirrors: db_subquery_queries_test.cpp

    template <>
    struct QueryProducer<subq::SubqueryInWhere> {
        template <db::IsSqlDialect DialectT, db::ParamMode DefaultMode>
        static db::CompiledDynamicQuery produce(const TestSchemas& s, db::QueryCompiler<DialectT, DefaultMode>& c) {
            using namespace db;
            // Mirrors: subquery in WHERE clause
            auto active_users = select(s.users.column<"id">()).from(s.users).where(s.users.column<"active">() == true);
            auto query        = select(s.posts.column<"title">())
                             .from(s.posts)
                             .where(in(s.posts.column<"user_id">(), subquery(active_users)));
            return c.compile_dynamic(query);
        }
    };

    template <>
    struct QueryProducer<subq::Exists> {
        template <db::IsSqlDialect DialectT, db::ParamMode DefaultMode>
        static db::CompiledDynamicQuery produce(const TestSchemas& s, db::QueryCompiler<DialectT, DefaultMode>& c) {
            using namespace db;
            // Mirrors: EXISTS expression
            auto published_posts = select(1).from(s.posts).where(
                s.posts.column<"user_id">() == s.users.column<"id">() && s.posts.column<"published">() == true);
            auto query = select(s.users.column<"name">()).from(s.users).where(exists(published_posts));
            return c.compile_dynamic(query);
        }
    };

    template <>
    struct QueryProducer<subq::NotExists> {
        template <db::IsSqlDialect DialectT, db::ParamMode DefaultMode>
        static db::CompiledDynamicQuery produce(const TestSchemas& s, db::QueryCompiler<DialectT, DefaultMode>& c) {
            using namespace db;
            // Mirrors: NOT EXISTS expression
            auto pending_orders = select(1).from(s.orders).where(
                s.orders.column<"user_id">() == s.users.column<"id">() && s.orders.column<"completed">() == false);
            auto query = select(s.users.column<"name">()).from(s.users).where(!exists(pending_orders));
            return c.compile_dynamic(query);
        }
    };

    template <>
    struct QueryProducer<subq::BasicSubquery> {
        template <db::IsSqlDialect DialectT, db::ParamMode DefaultMode>
        static db::CompiledDynamicQuery produce(const TestSchemas& s, db::QueryCompiler<DialectT, DefaultMode>& c) {
            using namespace db;
            // Mirrors: basic subquery compilation
            auto post_count_subquery = select(count(s.posts.column<"id">()))
                                           .from(s.posts)
                                           .where(s.posts.column<"user_id">() == s.users.column<"id">());
            auto query = subquery(post_count_subquery);
            return c.compile_dynamic(query);
        }
    };

    template <>
    struct QueryProducer<subq::SubqueryStructure> {
        template <db::IsSqlDialect DialectT, db::ParamMode DefaultMode>
        static db::CompiledDynamicQuery produce(const TestSchemas& s, db::QueryCompiler<DialectT, DefaultMode>& c) {
            using namespace db;
            // Mirrors: SubqueryStructureExpression
            auto user_post_count = select(count(s.posts.column<"id">()))
                                       .from(s.posts)
                                       .where(s.posts.column<"user_id">() == s.users.column<"id">());
            auto sub = subquery(user_post_count);
            return c.compile_dynamic(sub);
        }
    };

    template <>
    struct QueryProducer<subq::InSubqueryMultiple> {
        template <db::IsSqlDialect DialectT, db::ParamMode DefaultMode>
        static db::CompiledDynamicQuery produce(const TestSchemas& s, db::QueryCompiler<DialectT, DefaultMode>& c) {
            using namespace db;
            // Mirrors: IN with multiple values subquery (high value users)
            auto high_value_users = select(s.users.column<"id">())
                                        .from(s.orders)
                                        .where(s.orders.column<"amount">() > lit(1000.0))
                                        .group_by(s.orders.column<"user_id">())
                                        .having(sum(s.orders.column<"amount">()) > lit(5000.0));
            auto query = select(s.users.column<"name">())
                             .from(s.users)
                             .where(in(s.users.column<"id">(), subquery(high_value_users)));
            return c.compile_dynamic(query);
        }
    };

    template <>
    struct QueryProducer<subq::NestedSubqueries> {
        template <db::IsSqlDialect DialectT, db::ParamMode DefaultMode>
        static db::CompiledDynamicQuery produce(const TestSchemas& s, db::QueryCompiler<DialectT, DefaultMode>& c) {
            using namespace db;
            // Mirrors: nested subqueries
            auto users_with_completed_orders =
                select(s.orders.column<"user_id">()).from(s.orders).where(s.orders.column<"completed">() == true);
            auto posts_by_active_users =
                select(s.posts.column<"user_id">())
                    .from(s.posts)
                    .where(in(s.posts.column<"user_id">(), subquery(users_with_completed_orders)));
            auto query = select(s.users.column<"name">())
                             .from(s.users)
                             .where(in(s.users.column<"id">(), subquery(posts_by_active_users)));
            return c.compile_dynamic(query);
        }
    };

    template <>
    struct QueryProducer<subq::SubqueryWithAggregates> {
        template <db::IsSqlDialect DialectT, db::ParamMode DefaultMode>
        static db::CompiledDynamicQuery produce(const TestSchemas& s, db::QueryCompiler<DialectT, DefaultMode>& c) {
            using namespace db;
            // Mirrors: subquery with aggregates
            auto avg_order_amount =
                select(avg(s.orders.column<"amount">())).from(s.orders).where(s.orders.column<"completed">() == true);
            auto query = select(s.users.column<"name">(), s.orders.column<"amount">())
                             .from(s.users)
                             .join(s.orders)
                             .on(s.orders.column<"user_id">() == s.users.column<"id">())
                             .where(s.orders.column<"amount">() > subquery(avg_order_amount));
            return c.compile_dynamic(query);
        }
    };

    template <>
    struct QueryProducer<subq::SubqueryWithDistinct> {
        template <db::IsSqlDialect DialectT, db::ParamMode DefaultMode>
        static db::CompiledDynamicQuery produce(const TestSchemas& s, db::QueryCompiler<DialectT, DefaultMode>& c) {
            using namespace db;
            // Mirrors: subquery with DISTINCT
            auto unique_publishers =
                select_distinct(s.posts.column<"user_id">()).from(s.posts).where(s.posts.column<"published">() == true);
            auto query = select(s.users.column<"name">())
                             .from(s.users)
                             .where(in(s.users.column<"id">(), subquery(unique_publishers)));
            return c.compile_dynamic(query);
        }
    };

}  // namespace demiplane::test
