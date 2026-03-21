#include "test_schemas.hpp"

#include <postgres_sql_type_registry.hpp>
#include <postgres_type_mapping.hpp>
#include <providers.hpp>

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

    std::string_view SchemaDDL::users_table(db::Providers dialect) {
        switch (dialect) {
            case db::Providers::PostgreSQL:
                return pgsql_ddl::users_table;
            default:
                std::unreachable();
        }
    }

    std::string_view SchemaDDL::users_extended_table(db::Providers dialect) {
        switch (dialect) {
            case db::Providers::PostgreSQL:
                return pgsql_ddl::users_extended_table;
            default:
                std::unreachable();
        }
    }

    std::string_view SchemaDDL::posts_table(db::Providers dialect) {
        switch (dialect) {
            case db::Providers::PostgreSQL:
                return pgsql_ddl::posts_table;
            default:
                std::unreachable();
        }
    }

    std::string_view SchemaDDL::orders_table(db::Providers dialect) {
        switch (dialect) {
            case db::Providers::PostgreSQL:
                return pgsql_ddl::orders_table;
            default:
                std::unreachable();
        }
    }

    std::string_view SchemaDDL::orders_extended_table(db::Providers dialect) {
        switch (dialect) {
            case db::Providers::PostgreSQL:
                return pgsql_ddl::orders_extended_table;
            default:
                std::unreachable();
        }
    }

    std::string_view SchemaDDL::comments_table(db::Providers dialect) {
        switch (dialect) {
            case db::Providers::PostgreSQL:
                return pgsql_ddl::comments_table;
            default:
                std::unreachable();
        }
    }

    std::string_view SchemaDDL::drop_all(db::Providers dialect) {
        switch (dialect) {
            case db::Providers::PostgreSQL:
                return pgsql_ddl::drop_all;
            default:
                std::unreachable();
        }
    }

    TestSchemas TestSchemas::create(db::Providers provider) {
        // Build a DynamicTable for Record-based tests
        auto users_dyn = std::make_shared<db::DynamicTable>("users", provider);
        users_dyn->add_field<int>("id")
            .primary_key("id")
            .add_field<std::string>("name")
            .add_field<int>("age")
            .add_field<bool>("active");

        return TestSchemas{
            .users           = UsersTable{provider},
            .users_extended  = UsersExtendedTable{provider},
            .posts           = PostsTable{provider},
            .orders          = OrdersTable{provider},
            .orders_extended = OrdersExtendedTable{provider},
            .comments        = CommentsTable{provider},
            .users_dynamic   = std::move(users_dyn),
        };
    }

}  // namespace demiplane::test
