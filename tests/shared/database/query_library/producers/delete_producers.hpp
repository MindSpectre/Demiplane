#pragma once

#include <query_expressions.hpp>

#include "../query_producer.hpp"

namespace demiplane::test {

    // Mirrors: db_delete_queries_test.cpp

    template <>
    struct QueryProducer<del::BasicDelete> {
        static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
            using namespace db;
            // Mirrors: delete_from(users_schema).where(user_active == false)
            auto query = delete_from(s.users().table).where(s.users().active == false);
            return c.compile(query);
        }
    };

    template <>
    struct QueryProducer<del::DeleteWithTableName> {
        static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
            using namespace db;
            // Mirrors: delete_from("users").where(user_id > 0)
            auto query = delete_from("users").where(s.users().id > 0);
            return c.compile(query);
        }
    };

    template <>
    struct QueryProducer<del::DeleteWithoutWhere> {
        static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
            using namespace db;
            // Mirrors: delete_from(users_schema)
            auto query = delete_from(s.users().table);
            return c.compile(query);
        }
    };

    template <>
    struct QueryProducer<del::DeleteWhere> {
        static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
            using namespace db;
            // Mirrors: delete_from(users_schema).where(user_active == false)
            auto query = delete_from(s.users().table).where(s.users().active == false);
            return c.compile(query);
        }
    };

    template <>
    struct QueryProducer<del::DeleteComplexWhere> {
        static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
            using namespace db;
            // Mirrors: delete_from(users_schema).where(user_active == false && user_age < 18)
            auto query = delete_from(s.users().table).where(s.users().active == false && s.users().age < 18);
            return c.compile(query);
        }
    };

    template <>
    struct QueryProducer<del::DeleteWithIn> {
        static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
            using namespace db;
            // Mirrors: delete_from(users_schema).where(in(user_age, 18, 19, 20))
            auto query = delete_from(s.users().table).where(in(s.users().age, 18, 19, 20));
            return c.compile(query);
        }
    };

    template <>
    struct QueryProducer<del::DeleteWithBetween> {
        static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
            using namespace db;
            // Mirrors: delete_from(users_schema).where(between(user_age, 18, 25))
            auto query = delete_from(s.users().table).where(between(s.users().age, 18, 25));
            return c.compile(query);
        }
    };

    template <>
    struct QueryProducer<del::DeleteWithSubquery> {
        static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
            using namespace db;
            // Mirrors: delete with subquery
            auto inactive_users = select(s.users().id).from(s.users().table).where(s.users().active == false);
            auto query          = delete_from(s.users().table).where(in(s.users().id, subquery(inactive_users)));
            return c.compile(query);
        }
    };

}  // namespace demiplane::test
