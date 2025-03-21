#include "basic_mock_db_client.hpp"
#define ENABLE_TRACING
demiplane::database::BasicMockDbClient::~BasicMockDbClient() {}
void demiplane::database::BasicMockDbClient::create_database(std::string_view host, uint32_t port,
    std::string_view db_name, std::string_view login, std::string_view password) {
    TRACE_INFO(tracer_, "create_database()");
}
void demiplane::database::BasicMockDbClient::create_database(const ConnectParams& pr) {}
void demiplane::database::BasicMockDbClient::start_transaction() {
    TRACE_INFO(tracer_, "BasicMockDbClient::start_transaction()");
}
void demiplane::database::BasicMockDbClient::commit_transaction() {
    TRACE_INFO(tracer_, "BasicMockDbClient::commit_transaction()");
}
void demiplane::database::BasicMockDbClient::rollback_transaction() {
    TRACE_INFO(tracer_, "BasicMockDbClient::rollback_transaction()");
}
void demiplane::database::BasicMockDbClient::connect(const ConnectParams& params) {
    TRACE_INFO(tracer_, "BasicMockDbClient::connect()");
}
void demiplane::database::BasicMockDbClient::drop_connect() {
    TRACE_INFO(tracer_, "BasicMockDbClient::drop_connect()");
}
void demiplane::database::BasicMockDbClient::create_table(const query::CreateQuery& proposal) {
    TRACE_INFO(tracer_, "BasicMockDbClient::create_table()");
}
void demiplane::database::BasicMockDbClient::delete_table(std::string_view table_name) {
    TRACE_INFO(tracer_, "BasicMockDbClient::delete_table()");
}
void demiplane::database::BasicMockDbClient::truncate_table(std::string_view table_name) {
    TRACE_INFO(tracer_, "BasicMockDbClient::truncate_table()");
}
bool demiplane::database::BasicMockDbClient::check_table(std::string_view table_name) {
    TRACE_INFO(tracer_, "BasicMockDbClient::check_table()");
    return true;
}
void demiplane::database::BasicMockDbClient::make_unique_constraint(
    std::string_view table_name, FieldCollection key_fields) {
    TRACE_INFO(tracer_, "BasicMockDbClient::make_unique_constraint()");
}
void demiplane::database::BasicMockDbClient::setup_search_index(std::string_view table_name, FieldCollection fields) {
    TRACE_INFO(tracer_, "BasicMockDbClient::setup_search_index()");
}
void demiplane::database::BasicMockDbClient::drop_search_index(std::string_view table_name) const {
    TRACE_INFO(tracer_, "BasicMockDbClient::drop_search_index()");
}
void demiplane::database::BasicMockDbClient::remove_search_index(std::string_view table_name) {
    TRACE_INFO(tracer_, "BasicMockDbClient::remove_search_index()");
}
void demiplane::database::BasicMockDbClient::restore_search_index(std::string_view table_name) const {
    TRACE_INFO(tracer_, "BasicMockDbClient::restore_search_index()");
}
void demiplane::database::BasicMockDbClient::insert(query::InsertQuery&& query) {
    TRACE_INFO(tracer_, "BasicMockDbClient::insert()");
}
void demiplane::database::BasicMockDbClient::upsert(query::UpsertQuery&& query) {
    TRACE_INFO(tracer_, "BasicMockDbClient::upsert()");
}
demiplane::database::Records demiplane::database::BasicMockDbClient::insert_with_returning(query::InsertQuery&& query) {
    TRACE_INFO(tracer_, "BasicMockDbClient::insert_with_returning()");
    return {};
}
demiplane::database::Records demiplane::database::BasicMockDbClient::upsert_with_returning(query::UpsertQuery&& query) {
    TRACE_INFO(tracer_, "BasicMockDbClient::upsert_with_returning()");
    return {};
}
demiplane::database::Records demiplane::database::BasicMockDbClient::select(
    const query::SelectQuery& conditions) const {
    TRACE_INFO(tracer_, "BasicMockDbClient::select()");
    return {};
}
void demiplane::database::BasicMockDbClient::remove(const query::DeleteQuery& conditions) {
    TRACE_INFO(tracer_, "BasicMockDbClient::remove()");
}
uint32_t demiplane::database::BasicMockDbClient::count(const query::CountQuery& conditions) const {
    TRACE_INFO(tracer_, "BasicMockDbClient::count()");
    return 0;
}
void demiplane::database::BasicMockDbClient::set_search_fields(std::string_view table_name, FieldCollection fields) {
    TRACE_INFO(tracer_, "BasicMockDbClient::set_search_fields()");
}
void demiplane::database::BasicMockDbClient::set_conflict_fields(std::string_view table_name, FieldCollection fields) {
    TRACE_INFO(tracer_, "BasicMockDbClient::set_conflict_fields()");
}
