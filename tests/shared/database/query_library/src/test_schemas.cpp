#include "test_schemas.hpp"

#include <postgres_sql_type_registry.hpp>
#include <postgres_type_mapping.hpp>
#include <supported_providers.hpp>

// TODO: rewrite DDL generation using schema definitions (questionable - DDL might stay separate)
namespace demiplane::test {

    // DDL Strings for PostgreSQL
    namespace pgsql_ddl {
        constexpr std::string_view users_table = R"(
CREATE TABLE IF NOT EXISTS users (
    id SERIAL PRIMARY KEY,
    name VARCHAR(255),
    age INTEGER,
    active BOOLEAN
))";

        constexpr std::string_view users_extended_table = R"(
CREATE TABLE IF NOT EXISTS users (
    id SERIAL PRIMARY KEY,
    name VARCHAR(255),
    age INTEGER,
    active BOOLEAN,
    department VARCHAR(100),
    salary DECIMAL(10,2)
))";

        constexpr std::string_view posts_table = R"(
CREATE TABLE IF NOT EXISTS posts (
    id SERIAL PRIMARY KEY,
    user_id INTEGER,
    title VARCHAR(255),
    published BOOLEAN
))";

        constexpr std::string_view orders_table = R"(
CREATE TABLE IF NOT EXISTS orders (
    id SERIAL PRIMARY KEY,
    user_id INTEGER,
    amount DECIMAL(10,2),
    completed BOOLEAN
))";

        constexpr std::string_view orders_extended_table = R"(
CREATE TABLE IF NOT EXISTS orders (
    id SERIAL PRIMARY KEY,
    user_id INTEGER,
    amount DECIMAL(10,2),
    completed BOOLEAN,
    status VARCHAR(50),
    created_date DATE
))";

        constexpr std::string_view comments_table = R"(
CREATE TABLE IF NOT EXISTS comments (
    id SERIAL PRIMARY KEY,
    post_id INTEGER,
    user_id INTEGER,
    content TEXT
))";

        constexpr std::string_view drop_all = R"(
DROP TABLE IF EXISTS comments;
DROP TABLE IF EXISTS orders;
DROP TABLE IF EXISTS posts;
DROP TABLE IF EXISTS users;
)";
    }  // namespace pgsql_ddl

    std::string_view SchemaDDL::users_table(db::SupportedProviders dialect) {
        switch (dialect) {
            case db::SupportedProviders::PostgreSQL:
                return pgsql_ddl::users_table;
            default:
                std::unreachable();
        }
    }

    std::string_view SchemaDDL::users_extended_table(db::SupportedProviders dialect) {
        switch (dialect) {
            case db::SupportedProviders::PostgreSQL:
                return pgsql_ddl::users_extended_table;
            default:
                std::unreachable();
        }
    }

    std::string_view SchemaDDL::posts_table(db::SupportedProviders dialect) {
        switch (dialect) {
            case db::SupportedProviders::PostgreSQL:
                return pgsql_ddl::posts_table;
            default:
                std::unreachable();
        }
    }

    std::string_view SchemaDDL::orders_table(db::SupportedProviders dialect) {
        switch (dialect) {
            case db::SupportedProviders::PostgreSQL:
                return pgsql_ddl::orders_table;
            default:
                std::unreachable();
        }
    }

    std::string_view SchemaDDL::orders_extended_table(db::SupportedProviders dialect) {
        switch (dialect) {
            case db::SupportedProviders::PostgreSQL:
                return pgsql_ddl::orders_extended_table;
            default:
                std::unreachable();
        }
    }

    std::string_view SchemaDDL::comments_table(db::SupportedProviders dialect) {
        switch (dialect) {
            case db::SupportedProviders::PostgreSQL:
                return pgsql_ddl::comments_table;
            default:
                std::unreachable();
        }
    }

    std::string_view SchemaDDL::drop_all(db::SupportedProviders dialect) {
        switch (dialect) {
            case db::SupportedProviders::PostgreSQL:
                return pgsql_ddl::drop_all;
            default:
                std::unreachable();
        }
    }

    TestSchemas TestSchemas::create(db::SupportedProviders provider) {
        TestSchemas schemas;
        schemas.initialize(provider);
        return schemas;
    }

    void TestSchemas::initialize(db::SupportedProviders provider) {
        // Users schema (basic)
        users_.table = std::make_shared<db::Table>("users");
        users_.table->add_field<int>("id", provider)
            .primary_key("id")
            .add_field<std::string>("name", provider)
            .add_field<int>("age", provider)
            .add_field<bool>("active", provider);
        users_.id     = users_.table->column<int>("id");
        users_.name   = users_.table->column<std::string>("name");
        users_.age    = users_.table->column<int>("age");
        users_.active = users_.table->column<bool>("active");

        // Users extended schema
        users_extended_.table = std::make_shared<db::Table>("users");
        users_extended_.table->add_field<int>("id", provider)
            .primary_key("id")
            .add_field<std::string>("name", provider)
            .add_field<int>("age", provider)
            .add_field<bool>("active", provider)
            .add_field<std::string>("department", provider)
            .add_field<double>("salary", provider);
        users_extended_.id         = users_extended_.table->column<int>("id");
        users_extended_.name       = users_extended_.table->column<std::string>("name");
        users_extended_.age        = users_extended_.table->column<int>("age");
        users_extended_.active     = users_extended_.table->column<bool>("active");
        users_extended_.department = users_extended_.table->column<std::string>("department");
        users_extended_.salary     = users_extended_.table->column<double>("salary");

        // Posts schema
        posts_.table = std::make_shared<db::Table>("posts");
        posts_.table->add_field<int>("id", provider)
            .primary_key("id")
            .add_field<int>("user_id", provider)
            .add_field<std::string>("title", provider)
            .add_field<bool>("published", provider);
        posts_.id        = posts_.table->column<int>("id");
        posts_.user_id   = posts_.table->column<int>("user_id");
        posts_.title     = posts_.table->column<std::string>("title");
        posts_.published = posts_.table->column<bool>("published");

        // Orders schema (basic)
        orders_.table = std::make_shared<db::Table>("orders");
        orders_.table->add_field<int>("id", provider)
            .primary_key("id")
            .add_field<int>("user_id", provider)
            .add_field<double>("amount", provider)
            .add_field<bool>("completed", provider);
        orders_.id        = orders_.table->column<int>("id");
        orders_.user_id   = orders_.table->column<int>("user_id");
        orders_.amount    = orders_.table->column<double>("amount");
        orders_.completed = orders_.table->column<bool>("completed");

        // Orders extended schema
        // TODO: DATE type requires FieldValue support (time_t, std::chrono::system_clock, etc.),
        //       provider sink mapping, dialect casting, and ResultBlock integration
        orders_extended_.table = std::make_shared<db::Table>("orders");
        orders_extended_.table->add_field<int>("id", provider)
            .primary_key("id")
            .add_field<int>("user_id", provider)
            .add_field<double>("amount", provider)
            .add_field<bool>("completed", provider)
            .add_field<std::string>("status", provider)
            .add_field<std::string>("created_date", std::string{db::postgres::SqlTypeRegistry::date});
        orders_extended_.id           = orders_extended_.table->column<int>("id");
        orders_extended_.user_id      = orders_extended_.table->column<int>("user_id");
        orders_extended_.amount       = orders_extended_.table->column<double>("amount");
        orders_extended_.completed    = orders_extended_.table->column<bool>("completed");
        orders_extended_.status       = orders_extended_.table->column<std::string>("status");
        orders_extended_.created_date = orders_extended_.table->column<std::string>("created_date");

        // Comments schema
        comments_.table = std::make_shared<db::Table>("comments");
        comments_.table->add_field<int>("id", provider)
            .primary_key("id")
            .add_field<int>("post_id", provider)
            .add_field<int>("user_id", provider)
            .add_field<std::string>("content", provider);
        comments_.id      = comments_.table->column<int>("id");
        comments_.post_id = comments_.table->column<int>("post_id");
        comments_.user_id = comments_.table->column<int>("user_id");
        comments_.content = comments_.table->column<std::string>("content");
    }

}  // namespace demiplane::test
