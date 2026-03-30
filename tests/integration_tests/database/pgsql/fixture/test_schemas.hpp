#pragma once

#include <memory>
#include <string_view>

#include <db_constraints.hpp>
#include <db_static_table.hpp>
#include <db_table.hpp>

namespace demiplane::test {
    using namespace db::constraints;

    // Static table type aliases
    using UsersTable = db::StaticTable<"users",
                                       db::StaticFieldSchema<int, "id", PrimaryKey, NotNull>,
                                       db::StaticFieldSchema<std::string, "name">,
                                       db::StaticFieldSchema<int, "age">,
                                       db::StaticFieldSchema<bool, "active">>;

    using UsersExtendedTable = db::StaticTable<"users",
                                               db::StaticFieldSchema<int, "id", PrimaryKey, NotNull>,
                                               db::StaticFieldSchema<std::string, "name">,
                                               db::StaticFieldSchema<int, "age">,
                                               db::StaticFieldSchema<bool, "active">,
                                               db::StaticFieldSchema<std::string, "department">,
                                               db::StaticFieldSchema<double, "salary">>;

    using PostsTable = db::StaticTable<"posts",
                                       db::StaticFieldSchema<int, "id", PrimaryKey, NotNull>,
                                       db::StaticFieldSchema<int, "user_id">,
                                       db::StaticFieldSchema<std::string, "title">,
                                       db::StaticFieldSchema<bool, "published">>;

    using OrdersTable = db::StaticTable<"orders",
                                        db::StaticFieldSchema<int, "id", PrimaryKey, NotNull>,
                                        db::StaticFieldSchema<int, "user_id">,
                                        db::StaticFieldSchema<double, "amount">,
                                        db::StaticFieldSchema<bool, "completed">>;

    using OrdersExtendedTable = db::StaticTable<"orders",
                                                db::StaticFieldSchema<int, "id", PrimaryKey, NotNull>,
                                                db::StaticFieldSchema<int, "user_id">,
                                                db::StaticFieldSchema<double, "amount">,
                                                db::StaticFieldSchema<bool, "completed">,
                                                db::StaticFieldSchema<std::string, "status">,
                                                db::StaticFieldSchema<std::string, "created_date">>;

    using CommentsTable = db::StaticTable<"comments",
                                          db::StaticFieldSchema<int, "id", PrimaryKey, NotNull>,
                                          db::StaticFieldSchema<int, "post_id">,
                                          db::StaticFieldSchema<int, "user_id">,
                                          db::StaticFieldSchema<std::string, "content">>;

    // DDL strings for schema setup
    struct SchemaDDL {
        static std::string_view users_table(db::Providers dialect);
        static std::string_view users_extended_table(db::Providers dialect);
        static std::string_view posts_table(db::Providers dialect);
        static std::string_view orders_table(db::Providers dialect);
        static std::string_view orders_extended_table(db::Providers dialect);
        static std::string_view comments_table(db::Providers dialect);
        static std::string_view drop_all(db::Providers dialect);
    };

    struct TestSchemas {
        UsersTable users;
        UsersExtendedTable users_extended;
        PostsTable posts;
        OrdersTable orders;
        OrdersExtendedTable orders_extended;
        CommentsTable comments;

        // DynamicTable for Record-based tests (DDL, insert with records, etc.)
        std::shared_ptr<db::DynamicTable> users_dynamic;

        static TestSchemas create(db::Providers provider);
    };

}  // namespace demiplane::test
