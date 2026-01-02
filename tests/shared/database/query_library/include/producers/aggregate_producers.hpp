#pragma once

#include "../query_producer.hpp"

#include <query_expressions.hpp>

namespace demiplane::test {

// Mirrors: db_aggregate_queries_test.cpp

template <>
struct QueryProducer<aggregate::Count> {
    static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
        using namespace db;
        // Mirrors: select(count(user_id)).from(users_schema)
        auto query = db::select(count(s.users().id)).from(s.users().table);
        return c.compile(query);
    }
};

template <>
struct QueryProducer<aggregate::Sum> {
    static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
        using namespace db;
        // Mirrors: select(sum(user_age)).from(users_schema)
        auto query = select(sum(s.users().age)).from(s.users().table);
        return c.compile(query);
    }
};

template <>
struct QueryProducer<aggregate::Avg> {
    static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
        using namespace db;
        // Mirrors: select(avg(user_age)).from(users_schema)
        auto query = select(avg(s.users().age)).from(s.users().table);
        return c.compile(query);
    }
};

template <>
struct QueryProducer<aggregate::Min> {
    static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
        using namespace db;
        // Mirrors: select(min(user_age)).from(users_schema)
        auto query = select(min(s.users().age)).from(s.users().table);
        return c.compile(query);
    }
};

template <>
struct QueryProducer<aggregate::Max> {
    static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
        using namespace db;
        // Mirrors: select(max(user_age)).from(users_schema)
        auto query = select(max(s.users().age)).from(s.users().table);
        return c.compile(query);
    }
};

template <>
struct QueryProducer<aggregate::AggregateWithAlias> {
    static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
        using namespace db;
        // Mirrors: select(count(...).as("total_users"), sum(...).as("total_age"), ...)
        auto query = select(count(s.users().id).as("total_users"),
                            sum(s.users().age).as("total_age"),
                            avg(s.users().age).as("avg_age"),
                            min(s.users().age).as("min_age"),
                            max(s.users().age).as("max_age"))
                         .from(s.users().table);
        return c.compile(query);
    }
};

template <>
struct QueryProducer<aggregate::CountDistinct> {
    static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
        using namespace db;
        // Mirrors: select(count_distinct(user_age)).from(users_schema)
        auto query = select(count_distinct(s.users().age)).from(s.users().table);
        return c.compile(query);
    }
};

template <>
struct QueryProducer<aggregate::CountAll> {
    static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
        using namespace db;
        // Mirrors: select(count_all()).from(users_schema)
        auto query = select(count_all()).from(s.users().table);
        return c.compile(query);
    }
};

template <>
struct QueryProducer<aggregate::AggregateGroupBy> {
    static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
        using namespace db;
        // Mirrors: select(user_active, count(user_id).as("user_count")).from(...).group_by(user_active)
        auto query = select(s.users().active, count(s.users().id).as("user_count"))
                         .from(s.users().table)
                         .group_by(s.users().active);
        return c.compile(query);
    }
};

template <>
struct QueryProducer<aggregate::AggregateHaving> {
    static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
        using namespace db;
        // Mirrors: ...group_by(user_active).having(count(user_id) > 5)
        auto query = select(s.users().active, count(s.users().id).as("user_count"))
                         .from(s.users().table)
                         .group_by(s.users().active)
                         .having(count(s.users().id) > 5);
        return c.compile(query);
    }
};

template <>
struct QueryProducer<aggregate::MultipleAggregates> {
    static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
        using namespace db;
        // Mirrors: select(count(...), sum(...), avg(...), min(...), max(...), count_distinct(...))
        auto query = select(count(s.users().id),
                            sum(s.users().age),
                            avg(s.users().age),
                            min(s.users().age),
                            max(s.users().age),
                            count_distinct(s.users().name))
                         .from(s.users().table);
        return c.compile(std::move(query));
    }
};

template <>
struct QueryProducer<aggregate::AggregateMixedTypes> {
    static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
        using namespace db;
        // Mirrors: select(user_name, count(...).as("count"), "literal_value", avg(...).as("avg_age"))
        auto query = select(s.users().name, count(s.users().id).as("count"), "literal_value", avg(s.users().age).as("avg_age"))
                         .from(s.users().table)
                         .group_by(s.users().name);
        return c.compile(query);
    }
};

}  // namespace demiplane::test
