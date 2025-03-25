#include <chrono>
#include <gtest/gtest.h>
#include <vector>

#include "pqxx_query_engine.hpp"

using namespace demiplane::database;


TEST(QueryEngine, TestEscapeIdentifier) {
    std::string test;
    EXPECT_NO_THROW(test = PqxxQueryEngine::escape_identifier("TestString"));
    EXPECT_EQ(test, "\"TestString\"");
}
TEST(QueryEngine, TestProcessInsertQuery) {
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
    EXPECT_EQ(PqxxQueryEngine::process_insert(insert_query).query, "INSERT INTO \"test-table\" (\"Name\", \"Age\") VALUES (Alice, 18), (Bob, 21) RETURNING \"Name\";");
    insert_query.use_params = true;
    EXPECT_EQ(PqxxQueryEngine::process_insert(insert_query).query, "INSERT INTO \"test-table\" (\"Name\", \"Age\") VALUES ($1, $2), ($3, $4) RETURNING \"Name\";");
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}