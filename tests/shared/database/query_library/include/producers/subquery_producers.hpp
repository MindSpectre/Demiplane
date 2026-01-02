#pragma once

#include "../query_producer.hpp"

#include <query_expressions.hpp>

namespace demiplane::test {

// Mirrors: db_subquery_queries_test.cpp

template <>
struct QueryProducer<subq::SubqueryInWhere> {
    static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
        using namespace db;
        // Mirrors: subquery in WHERE clause
        auto active_users = select(s.users().id).from(s.users().table).where(s.users().active == true);
        auto query = select(s.posts().title).from(s.posts().table).where(in(s.posts().user_id, subquery(active_users)));
        return c.compile(query);
    }
};

template <>
struct QueryProducer<subq::Exists> {
    static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
        using namespace db;
        // Mirrors: EXISTS expression
        auto published_posts = select(1).from(s.posts().table)
                                   .where(s.posts().user_id == s.users().id && s.posts().published == true);
        auto query = select(s.users().name).from(s.users().table).where(exists(published_posts));
        return c.compile(query);
    }
};

template <>
struct QueryProducer<subq::NotExists> {
    static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
        using namespace db;
        // Mirrors: NOT EXISTS expression
        auto pending_orders = select(1).from(s.orders().table)
                                  .where(s.orders().user_id == s.users().id && s.orders().completed == false);
        auto query = select(s.users().name).from(s.users().table).where(!exists(pending_orders));
        return c.compile(query);
    }
};

template <>
struct QueryProducer<subq::BasicSubquery> {
    static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
        using namespace db;
        // Mirrors: basic subquery compilation
        auto post_count_subquery = select(count(s.posts().id)).from(s.posts().table)
                                       .where(s.posts().user_id == s.users().id);
        auto query = subquery(post_count_subquery);
        return c.compile(query);
    }
};

template <>
struct QueryProducer<subq::SubqueryStructure> {
    static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
        using namespace db;
        // Mirrors: SubqueryStructureExpression
        auto user_post_count = select(count(s.posts().id)).from(s.posts().table)
                                   .where(s.posts().user_id == s.users().id);
        auto sub = subquery(user_post_count);
        return c.compile(sub);
    }
};

template <>
struct QueryProducer<subq::InSubqueryMultiple> {
    static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
        using namespace db;
        // Mirrors: IN with multiple values subquery (high value users)
        auto high_value_users = select(s.users().id)
                                    .from(s.orders().table)
                                    .where(s.orders().amount > lit(1000.0))
                                    .group_by(s.orders().user_id)
                                    .having(sum(s.orders().amount) > lit(5000.0));
        auto query = select(s.users().name).from(s.users().table).where(in(s.users().id, subquery(high_value_users)));
        return c.compile(query);
    }
};

template <>
struct QueryProducer<subq::NestedSubqueries> {
    static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
        using namespace db;
        // Mirrors: nested subqueries
        auto users_with_completed_orders = select(s.orders().user_id).from(s.orders().table)
                                               .where(s.orders().completed == true);
        auto posts_by_active_users = select(s.posts().user_id).from(s.posts().table)
                                         .where(in(s.posts().user_id, subquery(users_with_completed_orders)));
        auto query = select(s.users().name).from(s.users().table)
                         .where(in(s.users().id, subquery(posts_by_active_users)));
        return c.compile(query);
    }
};

template <>
struct QueryProducer<subq::SubqueryWithAggregates> {
    static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
        using namespace db;
        // Mirrors: subquery with aggregates
        auto avg_order_amount = select(avg(s.orders().amount)).from(s.orders().table)
                                    .where(s.orders().completed == true);
        auto query = select(s.users().name, s.orders().amount)
                         .from(s.users().table)
                         .join(s.orders().table->table_name())
                         .on(s.orders().user_id == s.users().id)
                         .where(s.orders().amount > subquery(avg_order_amount));
        return c.compile(query);
    }
};

template <>
struct QueryProducer<subq::SubqueryWithDistinct> {
    static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
        using namespace db;
        // Mirrors: subquery with DISTINCT
        auto unique_publishers = select_distinct(s.posts().user_id).from(s.posts().table)
                                     .where(s.posts().published == true);
        auto query = select(s.users().name).from(s.users().table)
                         .where(in(s.users().id, subquery(unique_publishers)));
        return c.compile(query);
    }
};

}  // namespace demiplane::test
