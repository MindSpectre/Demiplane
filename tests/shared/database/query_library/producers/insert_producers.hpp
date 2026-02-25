#pragma once

#include <db_record.hpp>
#include <query_expressions.hpp>

#include "../query_producer.hpp"

namespace demiplane::test {

    // Mirrors: db_insert_queries_test.cpp

    template <>
    struct QueryProducer<ins::BasicInsert> {
        static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
            using namespace db;
            // Mirrors: insert_into(users_schema).into({"name", "age", "active"}).values({...})
            auto query = insert_into(s.users().table)
                             .into({"name", "age", "active"})
                             .values({std::string{"John Doe"}, 25, true});
            return c.compile(query);
        }
    };

    template <>
    struct QueryProducer<ins::InsertWithTableName> {
        static db::CompiledQuery produce(const TestSchemas& /*s*/, db::QueryCompiler& c) {
            using namespace db;
            // Mirrors: insert_into("users").into({"name", "age"}).values({...})
            auto query = insert_into("users").into({"name", "age"}).values({std::string{"Jane Doe"}, 30});
            return c.compile(std::move(query));
        }
    };

    template <>
    struct QueryProducer<ins::InsertWithRecord> {
        static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
            using namespace db;
            // Mirrors: insert_into(users_schema).into({...}).values(test_record)
            Record test_record(s.users().table);
            test_record["name"].set(std::string("Bob Smith"));
            test_record["age"].set(35);
            test_record["active"].set(true);
            auto query = insert_into(s.users().table).into({"name", "age", "active"}).values(test_record);
            return c.compile(query);
        }
    };

    template <>
    struct QueryProducer<ins::InsertBatch> {
        static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
            using namespace db;
            // Mirrors: insert_into(users_schema).into({...}).batch(records)
            Record record1(s.users().table);
            record1["name"].set(std::string("User1"));
            record1["age"].set(25);
            record1["active"].set(true);

            Record record2(s.users().table);
            record2["name"].set(std::string("User2"));
            record2["age"].set(30);
            record2["active"].set(false);

            const std::vector records = {record1, record2};
            auto query                = insert_into(s.users().table).into({"name", "age", "active"}).batch(records);
            return c.compile(query);
        }
    };

    template <>
    struct QueryProducer<ins::InsertMultipleValues> {
        static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
            using namespace db;
            // Mirrors: insert_into(...).into({...}).values({...}).values({...})
            auto query = insert_into(s.users().table)
                             .into({"name", "age", "active"})
                             .values({std::string{"User1"}, 25, true})
                             .values({std::string{"User2"}, 30, false});
            return c.compile(query);
        }
    };

}  // namespace demiplane::test
