#pragma once

#include "../query_producer.hpp"

#include <query_expressions.hpp>

namespace demiplane::test {

// Mirrors: db_join_queries_test.cpp

template <>
struct QueryProducer<join::InnerJoin> {
    static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
        using namespace db;
        // Mirrors: select(user_name, post_title).from(users_schema).join(posts_schema).on(...)
        auto query = select(s.users().name, s.posts().title)
                         .from(s.users().table)
                         .join(s.posts().table)
                         .on(s.posts().user_id == s.users().id);
        return c.compile(query);
    }
};

template <>
struct QueryProducer<join::LeftJoin> {
    static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
        using namespace db;
        // Mirrors: ...join(posts_schema, JoinType::LEFT)...
        auto query = select(s.users().name, s.posts().title)
                         .from(s.users().table)
                         .join(s.posts().table, JoinType::LEFT)
                         .on(s.posts().user_id == s.users().id);
        return c.compile(query);
    }
};

template <>
struct QueryProducer<join::RightJoin> {
    static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
        using namespace db;
        // Mirrors: ...join(posts_schema, JoinType::RIGHT)...
        auto query = select(s.users().name, s.posts().title)
                         .from(s.users().table)
                         .join(s.posts().table, JoinType::RIGHT)
                         .on(s.posts().user_id == s.users().id);
        return c.compile(query);
    }
};

template <>
struct QueryProducer<join::FullJoin> {
    static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
        using namespace db;
        // Mirrors: ...join(posts_schema, JoinType::FULL)...
        auto query = select(s.users().name, s.posts().title)
                         .from(s.users().table)
                         .join(s.posts().table, JoinType::FULL)
                         .on(s.posts().user_id == s.users().id);
        return c.compile(query);
    }
};

template <>
struct QueryProducer<join::CrossJoin> {
    static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
        using namespace db;
        // Mirrors: ...join(posts_schema, JoinType::CROSS).on(user_id > 0)
        auto query = select(s.users().name, s.posts().title)
                         .from(s.users().table)
                         .join(s.posts().table, JoinType::CROSS)
                         .on(s.users().id > 0);
        return c.compile(query);
    }
};

template <>
struct QueryProducer<join::MultipleJoins> {
    static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
        using namespace db;
        // Mirrors: simple join (placeholder for multiple joins)
        auto query = select(s.users().name, s.posts().title)
                         .from(s.users().table)
                         .join(s.posts().table)
                         .on(s.posts().user_id == s.users().id);
        return c.compile(query);
    }
};

template <>
struct QueryProducer<join::JoinComplexCondition> {
    static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
        using namespace db;
        // Mirrors: ...on(post_user_id == user_id && post_published == true)
        auto query = select(s.users().name, s.posts().title)
                         .from(s.users().table)
                         .join(s.posts().table)
                         .on(s.posts().user_id == s.users().id && s.posts().published == true);
        return c.compile(query);
    }
};

template <>
struct QueryProducer<join::JoinWithWhere> {
    static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
        using namespace db;
        // Mirrors: ...join(...).on(...).where(user_active == true)
        auto query = select(s.users().name, s.posts().title)
                         .from(s.users().table)
                         .join(s.posts().table)
                         .on(s.posts().user_id == s.users().id)
                         .where(s.users().active == true);
        return c.compile(query);
    }
};

template <>
struct QueryProducer<join::JoinWithAggregates> {
    static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
        using namespace db;
        // Mirrors: select(user_name, count(post_id).as("post_count"))...
        auto query = select(s.users().name, count(s.posts().id).as("post_count"))
                         .from(s.users().table)
                         .join(s.posts().table, JoinType::LEFT)
                         .on(s.posts().user_id == s.users().id);
        return c.compile(query);
    }
};

template <>
struct QueryProducer<join::JoinWithOrderBy> {
    static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
        using namespace db;
        // Mirrors: ...order_by(asc(user_name), desc(post_title))
        auto query = select(s.users().name, s.posts().title)
                         .from(s.users().table)
                         .join(s.posts().table)
                         .on(s.posts().user_id == s.users().id)
                         .order_by(asc(s.users().name), desc(s.posts().title));
        return c.compile(query);
    }
};

}  // namespace demiplane::test
