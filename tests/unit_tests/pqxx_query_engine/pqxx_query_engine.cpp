#include "pqxx_query_engine.hpp"

#include <chrono>
#include <gtest/gtest.h>
#include <vector>

using namespace demiplane::database;


TEST(QueryEngine, TestEscapeIdentifier) {
    std::string test;
    EXPECT_NO_THROW(test = PqxxQueryEngine::escape_identifier("TestString"));
    EXPECT_EQ(test, "\"TestString\"");
}
TEST(QueryEngine, TestInsertQuery) {
    query::InsertQuery insert_query;
    Records records;
    Record record;
    record.push_back(utility_factory::unique_field<std::string>("Name", "Alice"));
    record.push_back(utility_factory::unique_field<std::string>("Age", "18"));
    records.push_back(std::move(record));
    record.clear();
    record.push_back(utility_factory::unique_field<std::string>("Name", "Bob"));
    record.push_back(utility_factory::unique_field<std::string>("Age", "21"));
    records.push_back(std::move(record));
    Column clm{"Name", SqlType::TEXT};
    insert_query.insert(std::move(records)).to("test-table").return_with({clm});
    insert_query.use_params = false;
    EXPECT_EQ(PqxxQueryEngine::process_insert(insert_query).query,
        "INSERT INTO \"test-table\" (\"Name\", \"Age\") VALUES (Alice, 18), (Bob, 21) RETURNING \"Name\";");
    insert_query.use_params = true;
    EXPECT_EQ(PqxxQueryEngine::process_insert(insert_query).query,
        "INSERT INTO \"test-table\" (\"Name\", \"Age\") VALUES ($1, $2), ($3, $4) RETURNING \"Name\";");
}
TEST(QueryEngine, TestSelectQuery) {
    query::SelectQuery select_query;
    Column cl1{"Name", SqlType::TEXT}, cl2{"Age", SqlType::TEXT};
    query::WhereClause clause{"Name", query::WhereClause::Operator::EQUAL, "Bob"};
    select_query.select({cl1, cl2}).from("test-table").limit(10).offset(10).order_by(cl2, false).where(clause);
    EXPECT_EQ(
        "SELECT \"Name\", \"Age\" FROM \"test-table\" WHERE \"Name\" = $1 ORDER BY \"Age\" DESC LIMIT 10 OFFSET 10;",
        PqxxQueryEngine::process_select(select_query).query);
}
TEST(QueryEngine, TestСreateQuery) {
    query::CreateQuery create_query;
    Column clm{"Name", SqlType::TEXT};
    create_query.columns({clm, clm}).table("test-table");
    EXPECT_EQ("CREATE TABLE \"test-table\" (\"Name\" TEXT, \"Name\" TEXT);",
        PqxxQueryEngine::process_create(create_query).query);
}
TEST(QueryEngine, TestUpsertQuery) {
    query::UpsertQuery upsert_query;
    Records records;
    Record record;
    record.push_back(utility_factory::unique_field<std::string>("Name", "Alice"));
    record.push_back(utility_factory::unique_field<std::string>("Age", "18"));
    records.push_back(std::move(record));
    record.clear();
    record.push_back(utility_factory::unique_field<std::string>("Name", "Bob"));
    record.push_back(utility_factory::unique_field<std::string>("Age", "21"));
    records.push_back(std::move(record));
    Column clm{"Name", SqlType::TEXT};
    upsert_query.new_values(std::move(records))
        .to("test-table")
        .return_with({clm})
        .when_conflict_in_these_columns({clm})
        .replace_these_columns({clm});
    upsert_query.use_params = false;
    EXPECT_EQ(PqxxQueryEngine::process_upsert(upsert_query).query,
        "INSERT INTO \"test-table\" (\"Name\", \"Age\") VALUES (Alice, 18), (Bob, 21) ON CONFLICT (\"Name\") DO UPDATE "
        "SET \"Name\" = EXCLUDED.\"Name\" RETURNING \"Name\";");
    upsert_query.use_params = true;
    EXPECT_EQ(PqxxQueryEngine::process_upsert(upsert_query).query,
        "INSERT INTO \"test-table\" (\"Name\", \"Age\") VALUES ($1, $2), ($3, $4) ON CONFLICT (\"Name\") DO UPDATE SET "
        "\"Name\" = EXCLUDED.\"Name\" RETURNING \"Name\";");
}
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
