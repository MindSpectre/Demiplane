#pragma once

#include "../query_producer.hpp"

#include <query_expressions.hpp>

namespace demiplane::test {

// Mirrors: db_update_queries_test.cpp

template <>
struct QueryProducer<upd::BasicUpdate> {
    static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
        using namespace db;
        // Mirrors: update(users_schema).set("active", false).where(user_age < 18)
        auto query = update(s.users().table).set("active", false).where(s.users().age < 18);
        return c.compile(query);
    }
};

template <>
struct QueryProducer<upd::UpdateWithTableName> {
    static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
        using namespace db;
        // Mirrors: update("users").set("active", true).where(user_id > 0)
        auto query = update("users").set("active", true).where(s.users().id > 0);
        return c.compile(query);
    }
};

template <>
struct QueryProducer<upd::UpdateMultipleSet> {
    static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
        using namespace db;
        // Mirrors: update(users_schema).set("active", false).set("age", 21).where(user_age < 18)
        auto query = update(s.users().table).set("active", false).set("age", 21).where(s.users().age < 18);
        return c.compile(query);
    }
};

template <>
struct QueryProducer<upd::UpdateInitializerList> {
    static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
        using namespace db;
        // Mirrors: update(users_schema).set({{"active", false}, {"age", 21}}).where(user_age < 18)
        auto query = update(s.users().table)
                         .set({
                             {"active", false},
                             {"age",    21   }
                         })
                         .where(s.users().age < 18);
        return c.compile(query);
    }
};

template <>
struct QueryProducer<upd::UpdateWithoutWhere> {
    static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
        using namespace db;
        // Mirrors: update(users_schema).set("active", true)
        auto query = update(s.users().table).set("active", true);
        return c.compile(query);
    }
};

template <>
struct QueryProducer<upd::UpdateVariousTypes> {
    static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
        using namespace db;
        // Mirrors: update(users_schema).set("name", ...).set("age", ...).set("active", ...).where(...)
        auto query = update(s.users().table)
                         .set("name", std::string("New Name"))
                         .set("age", 30)
                         .set("active", true)
                         .where(s.users().id == 1);
        return c.compile(query);
    }
};

}  // namespace demiplane::test
