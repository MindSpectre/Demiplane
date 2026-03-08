#pragma once

#include <query_expressions.hpp>

#include "../query_producer.hpp"

namespace demiplane::test {

    // CTE (Common Table Expression) query producers

    template <>
    struct QueryProducer<cte::BasicCte> {
        template <db::IsSqlDialect DialectT, db::ParamMode DefaultMode>
        static db::CompiledDynamicQuery produce(const TestSchemas& s, db::QueryCompiler<DialectT, DefaultMode>& c) {
            using namespace db;
            // Basic CTE: WITH active_users AS (SELECT ...) SELECT * FROM active_users
            auto cte         = with("active_users",
                            select(s.users.column<"id">(), s.users.column<"name">())
                                .from(s.users)
                                .where(s.users.column<"active">() == true));
            // Using unqualified column names (no context) for CTE result columns
            const auto query = select(col("id"), col("name")).from(cte);
            return c.compile_dynamic(query);
        }
    };

    template <>
    struct QueryProducer<cte::CteWithSelect> {
        template <db::IsSqlDialect DialectT, db::ParamMode DefaultMode>
        static db::CompiledDynamicQuery produce(const TestSchemas& s, db::QueryCompiler<DialectT, DefaultMode>& c) {
            using namespace db;
            // CTE with aggregation
            auto cte         = with("user_orders",
                            select(s.orders.column<"user_id">(), sum(s.orders.column<"amount">()).as("total_amount"))
                                .from(s.orders)
                                .where(s.orders.column<"completed">() == true)
                                .group_by(s.orders.column<"user_id">()));
            const auto query = select(col("user_id"), col("total_amount")).from(cte);
            return c.compile_dynamic(query);
        }
    };

    template <>
    struct QueryProducer<cte::CteWithJoin> {
        template <db::IsSqlDialect DialectT, db::ParamMode DefaultMode>
        static db::CompiledDynamicQuery produce(const TestSchemas& s, db::QueryCompiler<DialectT, DefaultMode>& c) {
            using namespace db;
            // CTE with filtered data
            auto cte         = with("published_posts",
                            select(s.posts.column<"id">(), s.posts.column<"title">(), s.posts.column<"user_id">())
                                .from(s.posts)
                                .where(s.posts.column<"published">() == true));
            const auto query = select(col("id"), col("title"), col("user_id")).from(cte);
            return c.compile_dynamic(query);
        }
    };

    template <>
    struct QueryProducer<cte::MultipleCtes> {
        template <db::IsSqlDialect DialectT, db::ParamMode DefaultMode>
        static db::CompiledDynamicQuery produce(const TestSchemas& s, db::QueryCompiler<DialectT, DefaultMode>& c) {
            using namespace db;
            // Single CTE with aggregation (multiple CTEs not yet supported)
            auto cte         = with("post_stats",
                            select(s.posts.column<"user_id">(), count(s.posts.column<"id">()).as("post_count"))
                                .from(s.posts)
                                .group_by(s.posts.column<"user_id">()));
            const auto query = select(col("user_id"), col("post_count")).from(cte);
            return c.compile_dynamic(query);
        }
    };

    template <>
    struct QueryProducer<cte::CteWithAggregates> {
        template <db::IsSqlDialect DialectT, db::ParamMode DefaultMode>
        static db::CompiledDynamicQuery produce(const TestSchemas& s, db::QueryCompiler<DialectT, DefaultMode>& c) {
            using namespace db;
            // CTE with multiple aggregate functions
            auto cte = with("order_stats",
                            select(s.orders.column<"user_id">(),
                                   count(s.orders.column<"id">()).as("order_count"),
                                   sum(s.orders.column<"amount">()).as("total_spent"),
                                   avg(s.orders.column<"amount">()).as("avg_order"))
                                .from(s.orders)
                                .where(s.orders.column<"completed">() == true)
                                .group_by(s.orders.column<"user_id">()));
            const auto query =
                select(col("user_id"), col("order_count"), col("total_spent"), col("avg_order")).from(cte);
            return c.compile_dynamic(query);
        }
    };

}  // namespace demiplane::test
