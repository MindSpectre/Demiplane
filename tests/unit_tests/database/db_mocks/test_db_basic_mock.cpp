#include <gtest/gtest.h>
#include <memory>
#include "basic_mock_db_client.hpp"

using namespace demiplane::database;

class BasicMockDbClientTest : public ::testing::Test {
protected:
    BasicMockDbClient client;

    BasicMockDbClientTest() = default;

    static query::InsertQuery make_insert_query() {
        query::InsertQuery q{};
        // Fill with dummy fields if needed
        return q;
    }

    static query::SelectQuery make_select_query() {
        query::SelectQuery q{};
        // Fill with dummy fields
        return q;
    }

    static query::RemoveQuery make_remove_query() {
        query::RemoveQuery q{};
        return q;
    }

    static query::CreateTableQuery make_create_query() {
        query::CreateTableQuery q{};
        return q;
    }

    static query::UpsertQuery make_upsert_query() {
        query::UpsertQuery q{};
        return q;
    }

    static query::CountQuery make_count_query() {
        query::CountQuery q{};
        return q;
    }

    static FieldCollection make_fields() {
        return {};  // Add dummy fields if required
    }

};
TEST_F(BasicMockDbClientTest, CallAllMethods) {
    ConnectParams params;
    const auto db_config = std::make_shared<DatabaseConfig>();

    client.create_database(db_config, params);
    client.start_transaction();
    client.commit_transaction();
    client.rollback_transaction();
    client.connect(params);
    client.drop_connect();
    client.create_table(make_create_query());
    client.drop_table(query::DropTableQuery{"dummy_table"});
    client.truncate_table(query::TruncateTableQuery{"dummy_table"});

    EXPECT_NO_THROW({
        [[maybe_unused]] auto x = client.check_table(query::CheckTableQuery{"dummy_table"});
        client.insert(make_insert_query());
        client.upsert(make_upsert_query());
        auto selected = client.select(make_select_query());
        client.remove(make_remove_query());
        [[maybe_unused]] auto count = client.count(make_count_query());
    });
}
