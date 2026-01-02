#pragma once

#include "../query_producer.hpp"

#include <db_record.hpp>
#include <query_expressions.hpp>

namespace demiplane::test {

// Mirrors: db_select_queries_test.cpp

template <>
struct QueryProducer<sel::BasicSelect> {
    static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
        using namespace db;
        // Mirrors: select(user_id, user_name)
        auto query = select(s.users().id, s.users().name).from(s.users().table);
        return c.compile(query);
    }
};

template <>
struct QueryProducer<sel::SelectAllColumns> {
    static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
        using namespace db;
        // Mirrors: select(all("users"))
        auto query = select(all("users")).from(s.users().table);
        return c.compile(query);
    }
};

template <>
struct QueryProducer<sel::SelectDistinct> {
    static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
        using namespace db;
        // Mirrors: select_distinct(user_name, user_age)
        auto query = select_distinct(s.users().name, s.users().age).from(s.users().table);
        return c.compile(query);
    }
};

template <>
struct QueryProducer<sel::SelectMixedTypes> {
    static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
        using namespace db;
        // Mirrors: select(user_name, "constant", count(user_id).as("total")) with GROUP BY
        auto query = select(s.users().name, "constant", count(s.users().id).as("total"))
                         .from(s.users().table)
                         .group_by(s.users().name);
        return c.compile(query);
    }
};

template <>
struct QueryProducer<sel::SelectFromRecord> {
    static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
        using namespace db;
        // Mirrors: select(user_name).from(test_record)
        Record test_record(s.users().table);
        test_record["id"].set(1);
        test_record["name"].set(std::string("test"));
        auto query = select(s.users().name).from(test_record);
        return c.compile(query);
    }
};

template <>
struct QueryProducer<sel::SelectFromTableName> {
    static db::CompiledQuery produce(const TestSchemas& /*s*/, db::QueryCompiler& c) {
        using namespace db;
        // Mirrors: select(1).from("test_table")
        auto query = select(1).from("test_table");
        return c.compile(query);
    }
};

template <>
struct QueryProducer<sel::SelectWithWhere> {
    static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
        using namespace db;
        // Mirrors: select(user_name).from(users_schema).where(user_age > 18)
        auto query = select(s.users().name).from(s.users().table).where(s.users().age > 18);
        return c.compile(query);
    }
};

template <>
struct QueryProducer<sel::SelectWithJoin> {
    static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
        using namespace db;
        // Mirrors: select(user_name, post_title).from(users_schema).join(posts_schema->table_name()).on(post_user_id == user_id)
        auto query = select(s.users().name, s.posts().title)
                         .from(s.users().table)
                         .join(s.posts().table->table_name())
                         .on(s.posts().user_id == s.users().id);
        return c.compile(query);
    }
};

template <>
struct QueryProducer<sel::SelectWithGroupBy> {
    static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
        using namespace db;
        // Mirrors: select(user_active, count(user_id).as("user_count")).from(users_schema).group_by(user_active)
        auto query = select(s.users().active, count(s.users().id).as("user_count"))
                         .from(s.users().table)
                         .group_by(s.users().active);
        return c.compile(query);
    }
};

template <>
struct QueryProducer<sel::SelectWithHaving> {
    static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
        using namespace db;
        // Mirrors: select(...).from(...).group_by(...).having(count(user_id) > 5)
        auto query = select(s.users().active, count(s.users().id).as("user_count"))
                         .from(s.users().table)
                         .group_by(s.users().active)
                         .having(count(s.users().id) > 5);
        return c.compile(query);
    }
};

template <>
struct QueryProducer<sel::SelectWithOrderBy> {
    static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
        using namespace db;
        // Mirrors: select(user_name).from(users_schema).order_by(asc(user_name))
        auto query = select(s.users().name).from(s.users().table).order_by(asc(s.users().name));
        return c.compile(query);
    }
};

template <>
struct QueryProducer<sel::SelectWithLimit> {
    static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
        using namespace db;
        // Mirrors: select(user_name).from(users_schema).limit(10)
        auto query = select(s.users().name).from(s.users().table).limit(10);
        return c.compile(query);
    }
};

}  // namespace demiplane::test
