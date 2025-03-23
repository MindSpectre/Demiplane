#include "basic_mock_db_client.hpp"
#define ENABLE_TRACING
demiplane::database::BasicMockDbClient::BasicMockDbClient() {
    tracer_ = scroll::TracerFactory::create_default_console_tracer<BasicMockDbClient>();
    TRACE_INFO(tracer_, "BasicMockDbClient has been created.");
}
demiplane::database::BasicMockDbClient::~BasicMockDbClient() {
    TRACE_INFO(tracer_, "BasicMockDbClient has been destructed.");
}
demiplane::IRes<> demiplane::database::BasicMockDbClient::create_database(
    const std::shared_ptr<DatabaseConfig>& config, const ConnectParams& pr) {
    TRACE_INFO(tracer_, "BasicMockDbClient::create_database()");
    return IRes<>::sOk();
}

demiplane::IRes<> demiplane::database::BasicMockDbClient::start_transaction() {
    TRACE_INFO(tracer_, "BasicMockDbClient::start_transaction()");
    return IRes<>::sOk();
}
demiplane::IRes<> demiplane::database::BasicMockDbClient::commit_transaction() {
    TRACE_INFO(tracer_, "BasicMockDbClient::commit_transaction()");
    return IRes<>::sOk();
}
demiplane::IRes<> demiplane::database::BasicMockDbClient::rollback_transaction() {
    TRACE_INFO(tracer_, "BasicMockDbClient::rollback_transaction()");
    return IRes<>::sOk();
}
demiplane::IRes<> demiplane::database::BasicMockDbClient::connect(const ConnectParams& params) {
    TRACE_INFO(tracer_, "BasicMockDbClient::connect()");
    return IRes<>::sOk();
}
demiplane::IRes<> demiplane::database::BasicMockDbClient::drop_connect() {
    TRACE_INFO(tracer_, "BasicMockDbClient::drop_connect()");
    return IRes<>::sOk();
}
demiplane::IRes<> demiplane::database::BasicMockDbClient::create_table(const query::CreateQuery& proposal) {
    TRACE_INFO(tracer_, "BasicMockDbClient::create_table()");
    return IRes<>::sOk();
}
demiplane::IRes<> demiplane::database::BasicMockDbClient::delete_table(std::string_view table_name) {
    TRACE_INFO(tracer_, "BasicMockDbClient::delete_table()");
    return IRes<>::sOk();
}
demiplane::IRes<> demiplane::database::BasicMockDbClient::truncate_table(std::string_view table_name) {
    TRACE_INFO(tracer_, "BasicMockDbClient::truncate_table()");
    return IRes<>::sOk();

}
demiplane::IRes<bool> demiplane::database::BasicMockDbClient::check_table(std::string_view table_name) {
    TRACE_INFO(tracer_, "BasicMockDbClient::check_table()");
    return IRes<bool>{true};
}
demiplane::IRes<> demiplane::database::BasicMockDbClient::make_unique_constraint(
    std::string_view table_name, FieldCollection key_fields) {
    TRACE_INFO(tracer_, "BasicMockDbClient::make_unique_constraint()");
    return IRes<>::sOk();
}
demiplane::IRes<> demiplane::database::BasicMockDbClient::setup_search_index(std::string_view table_name, FieldCollection fields) {
    TRACE_INFO(tracer_, "BasicMockDbClient::setup_search_index()");
    return IRes<>::sOk();
}
demiplane::IRes<> demiplane::database::BasicMockDbClient::drop_search_index(std::string_view table_name) const {
    TRACE_INFO(tracer_, "BasicMockDbClient::drop_search_index()");
    return IRes<>::sOk();
}
demiplane::IRes<> demiplane::database::BasicMockDbClient::remove_search_index(std::string_view table_name) {
    TRACE_INFO(tracer_, "BasicMockDbClient::remove_search_index()");
    return IRes<>::sOk();
}
demiplane::IRes<> demiplane::database::BasicMockDbClient::restore_search_index(std::string_view table_name) const {
    TRACE_INFO(tracer_, "BasicMockDbClient::restore_search_index()");
    return IRes<>::sOk();
}
demiplane::IRes<> demiplane::database::BasicMockDbClient::insert(query::InsertQuery&& query) {
    TRACE_INFO(tracer_, "BasicMockDbClient::insert()");
    return IRes<>::sOk();
}
demiplane::IRes<> demiplane::database::BasicMockDbClient::upsert(query::UpsertQuery&& query) {
    TRACE_INFO(tracer_, "BasicMockDbClient::upsert()");
    return IRes<>::sOk();
}
demiplane::IRes<demiplane::database::Records> demiplane::database::BasicMockDbClient::insert_with_returning(query::InsertQuery&& query) {
    TRACE_INFO(tracer_, "BasicMockDbClient::insert_with_returning()");
    return {};
}
demiplane::IRes<demiplane::database::Records> demiplane::database::BasicMockDbClient::upsert_with_returning(query::UpsertQuery&& query) {
    TRACE_INFO(tracer_, "BasicMockDbClient::upsert_with_returning()");
    return {};
}
demiplane::IRes<demiplane::database::Records> demiplane::database::BasicMockDbClient::select(
    const query::SelectQuery& conditions) const {
    TRACE_INFO(tracer_, "BasicMockDbClient::select()");
    return demiplane::IRes<Records>{};
}
demiplane::IRes<> demiplane::database::BasicMockDbClient::remove(const query::DeleteQuery& conditions) {
    TRACE_INFO(tracer_, "BasicMockDbClient::remove()");
    return IRes<>::sOk();
}
demiplane::IRes<uint32_t> demiplane::database::BasicMockDbClient::count(const query::CountQuery& conditions) const {
    TRACE_INFO(tracer_, "BasicMockDbClient::count()");
    return IRes<uint32_t>{0};
}
void demiplane::database::BasicMockDbClient::set_search_fields(
    std::string_view table_name, [[maybe_unused]] FieldCollection fields) noexcept {
    TRACE_INFO(tracer_, "BasicMockDbClient::set_search_fields()");
}
void demiplane::database::BasicMockDbClient::set_conflict_fields(std::string_view table_name, [[maybe_unused]] FieldCollection fields) noexcept {
    TRACE_INFO(tracer_, "BasicMockDbClient::set_conflict_fields()");
}
