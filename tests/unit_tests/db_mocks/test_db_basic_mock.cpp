#include <gtest/gtest.h>
#include <memory>
#include "basic_mock_db_client.hpp"

using namespace demiplane::database;

class BasicMockDbClientTest : public ::testing::Test {
protected:
    std::shared_ptr<demiplane::scroll::TracerInterface> dummy_tracer = demiplane::scroll::TracerFactory::create_default_console_tracer<BasicMockDbClient>(); // use mock if needed
    BasicMockDbClient client;

    BasicMockDbClientTest() : client(dummy_tracer) {}

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

    static query::DeleteQuery make_delete_query() {
        query::DeleteQuery q{};
        return q;
    }

    static query::CreateQuery make_create_query() {
        query::CreateQuery q{};
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
    client.delete_table("dummy_table");
    client.truncate_table("dummy_table");

    EXPECT_NO_THROW({
        [[maybe_unused]] auto x = client.check_table("dummy_table");
        client.make_unique_constraint("dummy_table", make_fields());
        client.setup_search_index("dummy_table", make_fields());
        client.drop_search_index("dummy_table");
        client.remove_search_index("dummy_table");
        client.restore_search_index("dummy_table");
        client.insert(make_insert_query());
        client.upsert(make_upsert_query());
        auto inserted = client.insert_with_returning(make_insert_query());
        auto upserted = client.upsert_with_returning(make_upsert_query());
        auto selected = client.select(make_select_query());
        client.remove(make_delete_query());
        [[maybe_unused]] auto count = client.count(make_count_query());
        client.set_search_fields("dummy_table", make_fields());
        client.set_conflict_fields("dummy_table", make_fields());
    });
}
