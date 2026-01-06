#pragma once

#include <query_expressions.hpp>

#include "../query_producer.hpp"

namespace demiplane::test {

    // SET operation query producers

    template <>
    struct QueryProducer<set_op::UnionBasic> {
        static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
            using namespace db;
            // Basic UNION: removes duplicates
            auto active_users = select(s.users().name).from(s.users().table).where(s.users().active == true);
            auto young_users  = select(s.users().name).from(s.users().table).where(s.users().age < lit(30));
            auto query        = union_query(active_users, young_users);
            return c.compile(query);
        }
    };

    template <>
    struct QueryProducer<set_op::UnionAll> {
        static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
            using namespace db;
            // UNION ALL: keeps duplicates
            auto completed_orders =
                select(s.orders().user_id).from(s.orders().table).where(s.orders().completed == true);
            auto pending_orders =
                select(s.orders().user_id).from(s.orders().table).where(s.orders().completed == false);
            auto query = union_all(completed_orders, pending_orders);
            return c.compile(query);
        }
    };

    template <>
    struct QueryProducer<set_op::Intersect> {
        static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
            using namespace db;
            // INTERSECT: only rows in both result sets
            auto active_users      = select(s.users().id).from(s.users().table).where(s.users().active == true);
            auto users_with_orders = select(s.orders().user_id).from(s.orders().table);
            auto query             = intersect(active_users, users_with_orders);
            return c.compile(query);
        }
    };

    template <>
    struct QueryProducer<set_op::Except> {
        static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
            using namespace db;
            // EXCEPT: rows in first but not in second
            auto all_users        = select(s.users().id).from(s.users().table);
            auto users_with_posts = select(s.posts().user_id).from(s.posts().table);
            auto query            = except(all_users, users_with_posts);
            return c.compile(query);
        }
    };

    template <>
    struct QueryProducer<set_op::UnionWithOrderBy> {
        static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
            using namespace db;
            // UNION with ORDER BY using unqualified column name (no context)
            auto active_users =
                select(s.users().name, s.users().age).from(s.users().table).where(s.users().active == true);
            auto senior_users =
                select(s.users().name, s.users().age).from(s.users().table).where(s.users().age > lit(50));
            auto query = union_query(active_users, senior_users).order_by(desc(DynamicColumn("age")));
            return c.compile(query);
        }
    };

    template <>
    struct QueryProducer<set_op::UnionWithLimit> {
        static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
            using namespace db;
            // UNION with LIMIT
            auto small_orders =
                select(s.orders().id, s.orders().amount).from(s.orders().table).where(s.orders().amount < lit(100.0));
            auto large_orders =
                select(s.orders().id, s.orders().amount).from(s.orders().table).where(s.orders().amount > lit(500.0));
            auto query = union_query(small_orders, large_orders).limit(10);
            return c.compile(query);
        }
    };

    template <>
    struct QueryProducer<set_op::MultipleUnions> {
        static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
            using namespace db;
            // Chained UNIONs
            auto young = select(s.users().name).from(s.users().table).where(s.users().age < lit(25));
            auto middle =
                select(s.users().name).from(s.users().table).where(s.users().age >= lit(25) && s.users().age < lit(50));
            auto senior = select(s.users().name).from(s.users().table).where(s.users().age >= lit(50));
            auto query  = union_query(union_query(young, middle), senior);
            return c.compile(query);
        }
    };

    template <>
    struct QueryProducer<set_op::MixedSetOps> {
        static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
            using namespace db;
            // Mixed set operations: (A UNION B) EXCEPT C
            auto active      = select(s.users().id).from(s.users().table).where(s.users().active == true);
            auto with_orders = select(s.orders().user_id).from(s.orders().table).where(s.orders().completed == true);
            auto with_posts  = select(s.posts().user_id).from(s.posts().table).where(s.posts().published == true);
            auto query       = except(union_query(active, with_orders), with_posts);
            return c.compile(query);
        }
    };

}  // namespace demiplane::test
