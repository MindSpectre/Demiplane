#pragma once

#include <query_expressions.hpp>

#include "../query_producer.hpp"

namespace demiplane::test {

    // SET operation query producers

    template <>
    struct QueryProducer<set_op::UnionBasic> {
        template <db::IsSqlDialect DialectT, db::ParamMode DefaultMode>
        static db::CompiledDynamicQuery produce(const TestSchemas& s, db::QueryCompiler<DialectT, DefaultMode>& c) {
            using namespace db;
            // Basic UNION: removes duplicates
            auto active_users =
                select(s.users.column<"name">()).from(s.users).where(s.users.column<"active">() == true);
            auto young_users = select(s.users.column<"name">()).from(s.users).where(s.users.column<"age">() < lit(30));
            auto query       = union_query(active_users, young_users);
            return c.compile_dynamic(query);
        }
    };

    template <>
    struct QueryProducer<set_op::UnionAll> {
        template <db::IsSqlDialect DialectT, db::ParamMode DefaultMode>
        static db::CompiledDynamicQuery produce(const TestSchemas& s, db::QueryCompiler<DialectT, DefaultMode>& c) {
            using namespace db;
            // UNION ALL: keeps duplicates
            auto completed_orders =
                select(s.orders.column<"user_id">()).from(s.orders).where(s.orders.column<"completed">() == true);
            auto pending_orders =
                select(s.orders.column<"user_id">()).from(s.orders).where(s.orders.column<"completed">() == false);
            auto query = union_all(completed_orders, pending_orders);
            return c.compile_dynamic(query);
        }
    };

    template <>
    struct QueryProducer<set_op::Intersect> {
        template <db::IsSqlDialect DialectT, db::ParamMode DefaultMode>
        static db::CompiledDynamicQuery produce(const TestSchemas& s, db::QueryCompiler<DialectT, DefaultMode>& c) {
            using namespace db;
            // INTERSECT: only rows in both result sets
            auto active_users = select(s.users.column<"id">()).from(s.users).where(s.users.column<"active">() == true);
            auto users_with_orders = select(s.orders.column<"user_id">()).from(s.orders);
            auto query             = intersect(active_users, users_with_orders);
            return c.compile_dynamic(query);
        }
    };

    template <>
    struct QueryProducer<set_op::Except> {
        template <db::IsSqlDialect DialectT, db::ParamMode DefaultMode>
        static db::CompiledDynamicQuery produce(const TestSchemas& s, db::QueryCompiler<DialectT, DefaultMode>& c) {
            using namespace db;
            // EXCEPT: rows in first but not in second
            auto all_users        = select(s.users.column<"id">()).from(s.users);
            auto users_with_posts = select(s.posts.column<"user_id">()).from(s.posts);
            auto query            = except(all_users, users_with_posts);
            return c.compile_dynamic(query);
        }
    };

    template <>
    struct QueryProducer<set_op::UnionWithOrderBy> {
        template <db::IsSqlDialect DialectT, db::ParamMode DefaultMode>
        static db::CompiledDynamicQuery produce(const TestSchemas& s, db::QueryCompiler<DialectT, DefaultMode>& c) {
            using namespace db;
            // UNION with ORDER BY using unqualified column name (no context)
            auto active_users = select(s.users.column<"name">(), s.users.column<"age">())
                                    .from(s.users)
                                    .where(s.users.column<"active">() == true);
            auto senior_users = select(s.users.column<"name">(), s.users.column<"age">())
                                    .from(s.users)
                                    .where(s.users.column<"age">() > lit(50));
            auto query = union_query(active_users, senior_users).order_by(desc(col("age")));
            return c.compile_dynamic(query);
        }
    };

    template <>
    struct QueryProducer<set_op::UnionWithLimit> {
        template <db::IsSqlDialect DialectT, db::ParamMode DefaultMode>
        static db::CompiledDynamicQuery produce(const TestSchemas& s, db::QueryCompiler<DialectT, DefaultMode>& c) {
            using namespace db;
            // UNION with LIMIT
            auto small_orders = select(s.orders.column<"id">(), s.orders.column<"amount">())
                                    .from(s.orders)
                                    .where(s.orders.column<"amount">() < lit(100.0));
            auto large_orders = select(s.orders.column<"id">(), s.orders.column<"amount">())
                                    .from(s.orders)
                                    .where(s.orders.column<"amount">() > lit(500.0));
            auto query = union_query(small_orders, large_orders).limit(10);
            return c.compile_dynamic(query);
        }
    };

    template <>
    struct QueryProducer<set_op::MultipleUnions> {
        template <db::IsSqlDialect DialectT, db::ParamMode DefaultMode>
        static db::CompiledDynamicQuery produce(const TestSchemas& s, db::QueryCompiler<DialectT, DefaultMode>& c) {
            using namespace db;
            // Chained UNIONs
            auto young  = select(s.users.column<"name">()).from(s.users).where(s.users.column<"age">() < lit(25));
            auto middle = select(s.users.column<"name">())
                              .from(s.users)
                              .where(s.users.column<"age">() >= lit(25) && s.users.column<"age">() < lit(50));
            auto senior = select(s.users.column<"name">()).from(s.users).where(s.users.column<"age">() >= lit(50));
            auto query  = union_query(union_query(young, middle), senior);
            return c.compile_dynamic(query);
        }
    };

    template <>
    struct QueryProducer<set_op::MixedSetOps> {
        template <db::IsSqlDialect DialectT, db::ParamMode DefaultMode>
        static db::CompiledDynamicQuery produce(const TestSchemas& s, db::QueryCompiler<DialectT, DefaultMode>& c) {
            using namespace db;
            // Mixed set operations: (A UNION B) EXCEPT C
            auto active = select(s.users.column<"id">()).from(s.users).where(s.users.column<"active">() == true);
            auto with_orders =
                select(s.orders.column<"user_id">()).from(s.orders).where(s.orders.column<"completed">() == true);
            auto with_posts =
                select(s.posts.column<"user_id">()).from(s.posts).where(s.posts.column<"published">() == true);
            auto query = except(union_query(active, with_orders), with_posts);
            return c.compile_dynamic(query);
        }
    };

}  // namespace demiplane::test
