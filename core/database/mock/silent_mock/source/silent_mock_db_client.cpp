#include "silent_mock_db_client.hpp"
using namespace demiplane::database;
SilentMockDbClient::~SilentMockDbClient() {
    std::this_thread::sleep_for(utilities::chrono::RandomTimeGenerator::generate_milliseconds(30));
}
demiplane::IRes<> SilentMockDbClient::create_database(
    const std::shared_ptr<DatabaseConfig>& config, const ConnectParams& pr) {
    std::this_thread::sleep_for(utilities::chrono::RandomTimeGenerator::generate_milliseconds(200, 20));
    return IRes<>::sOk();
}
demiplane::IRes<> SilentMockDbClient::connect(const ConnectParams& params) {
    DbInterface::connect(params);
    return IRes<>::sOk();
}
demiplane::IRes<> SilentMockDbClient::start_transaction() {
    std::this_thread::sleep_for(utilities::chrono::RandomTimeGenerator::generate_milliseconds(5));
    return IRes<>::sOk();
}
demiplane::IRes<> SilentMockDbClient::commit_transaction() {
    std::this_thread::sleep_for(utilities::chrono::RandomTimeGenerator::generate_milliseconds(10));
    return IRes<>::sOk();
}
demiplane::IRes<> SilentMockDbClient::rollback_transaction() {
    std::this_thread::sleep_for(utilities::chrono::RandomTimeGenerator::generate_milliseconds(10));
    return IRes<>::sOk();
}
demiplane::IRes<> SilentMockDbClient::drop_connect() {
    std::this_thread::sleep_for(utilities::chrono::RandomTimeGenerator::generate_milliseconds(30));
    return IRes<>::sOk();
}
demiplane::IRes<> SilentMockDbClient::create_table(const query::CreateQuery& proposal) {
    std::this_thread::sleep_for(utilities::chrono::RandomTimeGenerator::generate_milliseconds(15));
    return IRes<>::sOk();
}
demiplane::IRes<> SilentMockDbClient::delete_table(std::string_view table_name) {
    std::this_thread::sleep_for(utilities::chrono::RandomTimeGenerator::generate_milliseconds(15));
    return IRes<>::sOk();
}
demiplane::IRes<> SilentMockDbClient::truncate_table(std::string_view table_name) {
    std::this_thread::sleep_for(utilities::chrono::RandomTimeGenerator::generate_milliseconds(15));
    return IRes<>::sOk();
}
demiplane::IRes<bool> SilentMockDbClient::check_table(std::string_view table_name) {
    std::this_thread::sleep_for(utilities::chrono::RandomTimeGenerator::generate_milliseconds(15));
    return IRes<bool>{true};
}

demiplane::IRes<std::optional<Records>> SilentMockDbClient::insert(query::InsertQuery&& query) {
    std::this_thread::sleep_for(utilities::chrono::RandomTimeGenerator::generate_milliseconds(100, 70));
    return IRes<std::optional<Records>>::sOk();
}
demiplane::IRes<std::optional<Records>> SilentMockDbClient::upsert(query::UpsertQuery&& query) {
    std::this_thread::sleep_for(utilities::chrono::RandomTimeGenerator::generate_milliseconds(120, 70));
    return IRes<std::optional<Records>>::sOk();
}
demiplane::IRes<Records> SilentMockDbClient::select(
    const query::SelectQuery& conditions) const {
    std::this_thread::sleep_for(utilities::chrono::RandomTimeGenerator::generate_milliseconds(250, 80));
    return demiplane::IRes<Records>{};
}
demiplane::IRes<std::optional<Records>> SilentMockDbClient::remove(const query::DeleteQuery& conditions) {
    std::this_thread::sleep_for(utilities::chrono::RandomTimeGenerator::generate_milliseconds(40));
    return IRes<std::optional<Records>>::sOk();
}
demiplane::IRes<unsigned> SilentMockDbClient::count(const query::CountQuery& conditions) const {
    std::this_thread::sleep_for(utilities::chrono::RandomTimeGenerator::generate_milliseconds(40));
    return IRes<unsigned>{0};
}
