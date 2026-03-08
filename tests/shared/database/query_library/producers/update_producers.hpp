#pragma once

#include <query_expressions.hpp>

#include "../query_producer.hpp"

namespace demiplane::test {

    // Mirrors: db_update_queries_test.cpp

    template <>
    struct QueryProducer<upd::BasicUpdate> {
        template <db::IsSqlDialect DialectT, db::ParamMode DefaultMode>
        static db::CompiledDynamicQuery produce(const TestSchemas& s, db::QueryCompiler<DialectT, DefaultMode>& c) {
            using namespace db;
            // Mirrors: update(users_schema).set("active", false).where(user_age < 18)
            auto query = update(s.users).set("active", false).where(s.users.column<"age">() < 18);
            return c.compile_dynamic(query);
        }
    };

    template <>
    struct QueryProducer<upd::UpdateWithTableName> {
        template <db::IsSqlDialect DialectT, db::ParamMode DefaultMode>
        static db::CompiledDynamicQuery produce(const TestSchemas& s, db::QueryCompiler<DialectT, DefaultMode>& c) {
            using namespace db;
            // Mirrors: update("users").set("active", true).where(user_id > 0)
            auto query = update("users").set("active", true).where(s.users.column<"id">() > 0);
            return c.compile_dynamic(query);
        }
    };

    template <>
    struct QueryProducer<upd::UpdateMultipleSet> {
        template <db::IsSqlDialect DialectT, db::ParamMode DefaultMode>
        static db::CompiledDynamicQuery produce(const TestSchemas& s, db::QueryCompiler<DialectT, DefaultMode>& c) {
            using namespace db;
            // Mirrors: update(users_schema).set("active", false).set("age", 21).where(user_age < 18)
            auto query = update(s.users).set("active", false).set("age", 21).where(s.users.column<"age">() < 18);
            return c.compile_dynamic(query);
        }
    };

    template <>
    struct QueryProducer<upd::UpdateInitializerList> {
        template <db::IsSqlDialect DialectT, db::ParamMode DefaultMode>
        static db::CompiledDynamicQuery produce(const TestSchemas& s, db::QueryCompiler<DialectT, DefaultMode>& c) {
            using namespace db;
            // Mirrors: update(users_schema).set({{"active", false}, {"age", 21}}).where(user_age < 18)
            auto query = update(s.users)
                             .set({
                                 {"active", false},
                                 {"age",    21   }
            })
                             .where(s.users.column<"age">() < 18);
            return c.compile_dynamic(query);
        }
    };

    template <>
    struct QueryProducer<upd::UpdateWithoutWhere> {
        template <db::IsSqlDialect DialectT, db::ParamMode DefaultMode>
        static db::CompiledDynamicQuery produce(const TestSchemas& s, db::QueryCompiler<DialectT, DefaultMode>& c) {
            using namespace db;
            // Mirrors: update(users_schema).set("active", true)
            auto query = update(s.users).set("active", true);
            return c.compile_dynamic(query);
        }
    };

    template <>
    struct QueryProducer<upd::UpdateVariousTypes> {
        template <db::IsSqlDialect DialectT, db::ParamMode DefaultMode>
        static db::CompiledDynamicQuery produce(const TestSchemas& s, db::QueryCompiler<DialectT, DefaultMode>& c) {
            using namespace db;
            // Mirrors: update(users_schema).set("name", ...).set("age", ...).set("active", ...).where(...)
            auto query = update(s.users)
                             .set("name", std::string("New Name"))
                             .set("age", 30)
                             .set("active", true)
                             .where(s.users.column<"id">() == 1);
            return c.compile_dynamic(query);
        }
    };

}  // namespace demiplane::test
