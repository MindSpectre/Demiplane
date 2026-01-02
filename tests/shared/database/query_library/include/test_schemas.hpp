#pragma once

#include <memory>
#include <string_view>

#include <db_column.hpp>
#include <db_table.hpp>

namespace demiplane::test {

    enum class DialectType { PostgreSQL, MySQL, SQLite };

    // DDL strings for schema setup (workaround until DDL builder exists)
    struct SchemaDDL {
        static std::string_view users_table(DialectType dialect);
        static std::string_view users_extended_table(DialectType dialect);
        static std::string_view posts_table(DialectType dialect);
        static std::string_view orders_table(DialectType dialect);
        static std::string_view orders_extended_table(DialectType dialect);
        static std::string_view comments_table(DialectType dialect);
        static std::string_view drop_all(DialectType dialect);
    };

    class TestSchemas {
    public:
        // users: id, name, age, active
        struct UsersSchema {
            std::shared_ptr<db::Table> table;
            db::TableColumn<int> id{nullptr, ""};
            db::TableColumn<std::string> name{nullptr, ""};
            db::TableColumn<int> age{nullptr, ""};
            db::TableColumn<bool> active{nullptr, ""};
        };

        // users_extended: id, name, age, active, department, salary
        struct UsersExtendedSchema {
            std::shared_ptr<db::Table> table;
            db::TableColumn<int> id{nullptr, ""};
            db::TableColumn<std::string> name{nullptr, ""};
            db::TableColumn<int> age{nullptr, ""};
            db::TableColumn<bool> active{nullptr, ""};
            db::TableColumn<std::string> department{nullptr, ""};
            db::TableColumn<double> salary{nullptr, ""};
        };

        // posts: id, user_id, title, published
        struct PostsSchema {
            std::shared_ptr<db::Table> table;
            db::TableColumn<int> id{nullptr, ""};
            db::TableColumn<int> user_id{nullptr, ""};
            db::TableColumn<std::string> title{nullptr, ""};
            db::TableColumn<bool> published{nullptr, ""};
        };

        // orders: id, user_id, amount, completed
        struct OrdersSchema {
            std::shared_ptr<db::Table> table;
            db::TableColumn<int> id{nullptr, ""};
            db::TableColumn<int> user_id{nullptr, ""};
            db::TableColumn<double> amount{nullptr, ""};
            db::TableColumn<bool> completed{nullptr, ""};
        };

        // orders_extended: id, user_id, amount, completed, status, created_date
        struct OrdersExtendedSchema {
            std::shared_ptr<db::Table> table;
            db::TableColumn<int> id{nullptr, ""};
            db::TableColumn<int> user_id{nullptr, ""};
            db::TableColumn<double> amount{nullptr, ""};
            db::TableColumn<bool> completed{nullptr, ""};
            db::TableColumn<std::string> status{nullptr, ""};
            db::TableColumn<std::string> created_date{nullptr, ""};
        };

        // comments: id, post_id, user_id, content
        struct CommentsSchema {
            std::shared_ptr<db::Table> table;
            db::TableColumn<int> id{nullptr, ""};
            db::TableColumn<int> post_id{nullptr, ""};
            db::TableColumn<int> user_id{nullptr, ""};
            db::TableColumn<std::string> content{nullptr, ""};
        };

        static TestSchemas create();

        [[nodiscard]] const UsersSchema& users() const {
            return users_;
        }
        [[nodiscard]] const UsersExtendedSchema& users_extended() const {
            return users_extended_;
        }
        [[nodiscard]] const PostsSchema& posts() const {
            return posts_;
        }
        [[nodiscard]] const OrdersSchema& orders() const {
            return orders_;
        }
        [[nodiscard]] const OrdersExtendedSchema& orders_extended() const {
            return orders_extended_;
        }
        [[nodiscard]] const CommentsSchema& comments() const {
            return comments_;
        }

    private:
        UsersSchema users_;
        UsersExtendedSchema users_extended_;
        PostsSchema posts_;
        OrdersSchema orders_;
        OrdersExtendedSchema orders_extended_;
        CommentsSchema comments_;

        void initialize();
    };

}  // namespace demiplane::test
