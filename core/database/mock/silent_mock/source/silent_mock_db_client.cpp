#include "silent_mock_db_client.hpp"
demiplane::database::SilentMockDbClient::~SilentMockDbClient() {
    std::this_thread::sleep_for(utilities::chrono::RandomTimeGenerator::generate_milliseconds(30));
}
void demiplane::database::SilentMockDbClient::start_transaction() {
    std::this_thread::sleep_for(utilities::chrono::RandomTimeGenerator::generate_milliseconds(5));
}
void demiplane::database::SilentMockDbClient::commit_transaction() {
    std::this_thread::sleep_for(utilities::chrono::RandomTimeGenerator::generate_milliseconds(10));
}
void demiplane::database::SilentMockDbClient::rollback_transaction() {
    std::this_thread::sleep_for(utilities::chrono::RandomTimeGenerator::generate_milliseconds(10));
}
void demiplane::database::SilentMockDbClient::drop_connect() {
    std::this_thread::sleep_for(utilities::chrono::RandomTimeGenerator::generate_milliseconds(30));
}
void demiplane::database::SilentMockDbClient::create_table(const query::CreateQuery& proposal) {
    std::this_thread::sleep_for(utilities::chrono::RandomTimeGenerator::generate_milliseconds(15));
}
void demiplane::database::SilentMockDbClient::delete_table(std::string_view table_name) {
    std::this_thread::sleep_for(utilities::chrono::RandomTimeGenerator::generate_milliseconds(15));
}
void demiplane::database::SilentMockDbClient::truncate_table(std::string_view table_name) {
    std::this_thread::sleep_for(utilities::chrono::RandomTimeGenerator::generate_milliseconds(15));
}
bool demiplane::database::SilentMockDbClient::check_table(std::string_view table_name) {
    std::this_thread::sleep_for(utilities::chrono::RandomTimeGenerator::generate_milliseconds(15));
    return true;
}
void demiplane::database::SilentMockDbClient::make_unique_constraint(
    std::string_view table_name, FieldCollection key_fields) {
    std::this_thread::sleep_for(utilities::chrono::RandomTimeGenerator::generate_milliseconds(20));
}
void demiplane::database::SilentMockDbClient::setup_search_index(std::string_view table_name, FieldCollection fields) {
    std::this_thread::sleep_for(utilities::chrono::RandomTimeGenerator::generate_milliseconds(120));
}
void demiplane::database::SilentMockDbClient::drop_search_index(std::string_view table_name) const {
    std::this_thread::sleep_for(utilities::chrono::RandomTimeGenerator::generate_milliseconds(30));
}
void demiplane::database::SilentMockDbClient::remove_search_index(std::string_view table_name) {
    std::this_thread::sleep_for(utilities::chrono::RandomTimeGenerator::generate_milliseconds(30));
}
void demiplane::database::SilentMockDbClient::restore_search_index(std::string_view table_name) const {
    std::this_thread::sleep_for(utilities::chrono::RandomTimeGenerator::generate_milliseconds(120));
}
void demiplane::database::SilentMockDbClient::insert(query::InsertQuery&& query) {
    std::this_thread::sleep_for(utilities::chrono::RandomTimeGenerator::generate_milliseconds(100, 70));
}
void demiplane::database::SilentMockDbClient::upsert(query::UpsertQuery&& query) {
    std::this_thread::sleep_for(utilities::chrono::RandomTimeGenerator::generate_milliseconds(120, 70));
}
demiplane::database::Records demiplane::database::SilentMockDbClient::insert_with_returning(
    query::InsertQuery&& query) {
    std::this_thread::sleep_for(utilities::chrono::RandomTimeGenerator::generate_milliseconds(200, 50));
    return {};
}
demiplane::database::Records demiplane::database::SilentMockDbClient::upsert_with_returning(
    query::UpsertQuery&& query) {
    std::this_thread::sleep_for(utilities::chrono::RandomTimeGenerator::generate_milliseconds(200, 50));
    return {};
}
demiplane::database::Records demiplane::database::SilentMockDbClient::select(
    const query::SelectQuery& conditions) const {
    std::this_thread::sleep_for(utilities::chrono::RandomTimeGenerator::generate_milliseconds(250, 80));
    return {};
}
void demiplane::database::SilentMockDbClient::remove(const query::DeleteQuery& conditions) {
    std::this_thread::sleep_for(utilities::chrono::RandomTimeGenerator::generate_milliseconds(40));
}
uint32_t demiplane::database::SilentMockDbClient::count(const query::CountQuery& conditions) const {
    std::this_thread::sleep_for(utilities::chrono::RandomTimeGenerator::generate_milliseconds(40));
    return 0;
}
void demiplane::database::SilentMockDbClient::set_search_fields(std::string_view table_name, FieldCollection fields) {
    std::this_thread::sleep_for(utilities::chrono::RandomTimeGenerator::generate_milliseconds(5));
}
void demiplane::database::SilentMockDbClient::set_conflict_fields(std::string_view table_name, FieldCollection fields) {
    std::this_thread::sleep_for(utilities::chrono::RandomTimeGenerator::generate_milliseconds(5));
}
