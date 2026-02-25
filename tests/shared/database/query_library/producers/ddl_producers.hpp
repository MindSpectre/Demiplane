#pragma once

#include <db_table.hpp>
#include <query_expressions.hpp>

#include "../query_producer.hpp"

namespace demiplane::test {

    // DDL Producers - CREATE TABLE and DROP TABLE expressions

    // ============== CREATE TABLE Producers ==============

    template <>
    struct QueryProducer<ddl::CreateTableBasic> {
        static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
            using namespace db;
            // Basic CREATE TABLE from existing schema
            auto query = create_table(s.users().table);
            return c.compile(query);
        }
    };

    template <>
    struct QueryProducer<ddl::CreateTableIfNotExists> {
        static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
            using namespace db;
            // CREATE TABLE IF NOT EXISTS
            auto query = create_table(s.users().table, true);
            return c.compile(query);
        }
    };

    template <>
    struct QueryProducer<ddl::CreateTableWithConstraints> {
        static db::CompiledQuery produce(const TestSchemas& /*s*/, db::QueryCompiler& c) {
            using namespace db;
            // Create a table with PK, NOT NULL, UNIQUE constraints
            auto table = std::make_shared<Table>("ddl_constraints_test");
            table->add_field<int>("id", "SERIAL").primary_key("id");
            table->add_field<std::string>("email", "VARCHAR(255)").nullable("email", false).unique("email");
            table->add_field<std::string>("name", "VARCHAR(100)").nullable("name", false);
            table->add_field<int>("status", "INTEGER");

            auto query = create_table(table, true);
            return c.compile(query);
        }
    };

    template <>
    struct QueryProducer<ddl::CreateTableWithForeignKey> {
        static db::CompiledQuery produce(const TestSchemas& /*s*/, db::QueryCompiler& c) {
            using namespace db;
            // Create a table with FOREIGN KEY constraint
            auto table = std::make_shared<Table>("ddl_orders_test");
            table->add_field<int>("id", "SERIAL").primary_key("id");
            table->add_field<int>("user_id", "INTEGER").foreign_key("user_id", "users", "id");
            table->add_field<double>("amount", "DECIMAL(10,2)");

            auto query = create_table(table, true);
            return c.compile(query);
        }
    };

    template <>
    struct QueryProducer<ddl::CreateTableWithDefault> {
        static db::CompiledQuery produce(const TestSchemas& /*s*/, db::QueryCompiler& c) {
            using namespace db;
            // Create a table with DEFAULT value
            auto table = std::make_shared<Table>("ddl_settings_test");
            table->add_field<int>("id", "SERIAL").primary_key("id");
            table->add_field<bool>("enabled", "BOOLEAN");
            table->add_field<int>("priority", "INTEGER");

            // Set default values via FieldSchema
            if (auto* field = table->get_field_schema("enabled")) {
                field->default_value = "true";
            }
            if (auto* field = table->get_field_schema("priority")) {
                field->default_value = "0";
            }

            auto query = create_table(table, true);
            return c.compile(query);
        }
    };

    // ============== DROP TABLE Producers ==============

    template <>
    struct QueryProducer<ddl::DropTableBasic> {
        static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
            using namespace db;
            // Basic DROP TABLE
            auto query = drop_table(s.users().table);
            return c.compile(query);
        }
    };

    template <>
    struct QueryProducer<ddl::DropTableIfExists> {
        static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
            using namespace db;
            // DROP TABLE IF EXISTS
            auto query = drop_table(s.users().table, true);
            return c.compile(query);
        }
    };

    template <>
    struct QueryProducer<ddl::DropTableCascade> {
        static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
            using namespace db;
            // DROP TABLE CASCADE
            auto query = drop_table(s.users().table, false, true);
            return c.compile(query);
        }
    };

    template <>
    struct QueryProducer<ddl::DropTableIfExistsCascade> {
        static db::CompiledQuery produce(const TestSchemas& s, db::QueryCompiler& c) {
            using namespace db;
            // DROP TABLE IF EXISTS CASCADE
            auto query = drop_table(s.users().table, true, true);
            return c.compile(query);
        }
    };

    template <>
    struct QueryProducer<ddl::DropTableByName> {
        static db::CompiledQuery produce(const TestSchemas& /*s*/, db::QueryCompiler& c) {
            using namespace db;
            // DROP TABLE by string name
            auto query = drop_table("ddl_temp_table", true, true);
            return c.compile(query);
        }
    };

}  // namespace demiplane::test
