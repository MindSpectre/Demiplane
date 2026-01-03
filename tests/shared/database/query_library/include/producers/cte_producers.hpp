#pragma once

#include "../query_producer.hpp"

#include <query_expressions.hpp>

namespace demiplane::test {

// CTE (Common Table Expression) query producers

template <>
struct QueryProducer<cte::BasicCte> {
    static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
        using namespace db;
        // Basic CTE: WITH active_users AS (SELECT ...) SELECT * FROM active_users
        auto cte = with("active_users",
                        select(s.users().id, s.users().name)
                            .from(s.users().table)
                            .where(s.users().active == true));
        // Using unqualified column names (no context) for CTE result columns
        auto query = select(DynamicColumn("id"), DynamicColumn("name")).from(cte);
        return c.compile(query);
    }
};

template <>
struct QueryProducer<cte::CteWithSelect> {
    static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
        using namespace db;
        // CTE with aggregation
        auto cte = with("user_orders",
                        select(s.orders().user_id, sum(s.orders().amount).as("total_amount"))
                            .from(s.orders().table)
                            .where(s.orders().completed == true)
                            .group_by(s.orders().user_id));
        auto query = select(DynamicColumn("user_id"), DynamicColumn("total_amount")).from(cte);
        return c.compile(query);
    }
};

template <>
struct QueryProducer<cte::CteWithJoin> {
    static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
        using namespace db;
        // CTE with filtered data
        auto cte = with("published_posts",
                        select(s.posts().id, s.posts().title, s.posts().user_id)
                            .from(s.posts().table)
                            .where(s.posts().published == true));
        auto query = select(DynamicColumn("id"), DynamicColumn("title"), DynamicColumn("user_id")).from(cte);
        return c.compile(query);
    }
};

template <>
struct QueryProducer<cte::MultipleCtes> {
    static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
        using namespace db;
        // Single CTE with aggregation (multiple CTEs not yet supported)
        auto cte = with("post_stats",
                        select(s.posts().user_id, count(s.posts().id).as("post_count"))
                            .from(s.posts().table)
                            .group_by(s.posts().user_id));
        auto query = select(DynamicColumn("user_id"), DynamicColumn("post_count")).from(cte);
        return c.compile(query);
    }
};

template <>
struct QueryProducer<cte::CteWithAggregates> {
    static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
        using namespace db;
        // CTE with multiple aggregate functions
        auto cte = with("order_stats",
                        select(s.orders().user_id,
                               count(s.orders().id).as("order_count"),
                               sum(s.orders().amount).as("total_spent"),
                               avg(s.orders().amount).as("avg_order"))
                            .from(s.orders().table)
                            .where(s.orders().completed == true)
                            .group_by(s.orders().user_id));
        auto query = select(DynamicColumn("user_id"),
                            DynamicColumn("order_count"),
                            DynamicColumn("total_spent"),
                            DynamicColumn("avg_order"))
                         .from(cte);
        return c.compile(query);
    }
};

}  // namespace demiplane::test
