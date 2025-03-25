#include "basic_mock_db_client.hpp"
using namespace demiplane::database;
#define ENABLE_TRACING

BasicMockDbClient::BasicMockDbClient() {
    tracer_ = scroll::TracerFactory::create_default_console_tracer<BasicMockDbClient>();
    TRACE_INFO(tracer_, "BasicMockDbClient has been created.");
}
BasicMockDbClient::~BasicMockDbClient() {
    TRACE_INFO(tracer_, "BasicMockDbClient has been destructed.");
}
demiplane::IRes<> BasicMockDbClient::create_database(
    const std::shared_ptr<DatabaseConfig>& config, const ConnectParams& pr) {
    TRACE_INFO(tracer_, "BasicMockDbClient::create_database()");
    return IRes<>::sOk();
}

demiplane::IRes<> BasicMockDbClient::start_transaction() {
    TRACE_INFO(tracer_, "BasicMockDbClient::start_transaction()");
    return IRes<>::sOk();
}
demiplane::IRes<> BasicMockDbClient::commit_transaction() {
    TRACE_INFO(tracer_, "BasicMockDbClient::commit_transaction()");
    return IRes<>::sOk();
}
demiplane::IRes<> BasicMockDbClient::rollback_transaction() {
    TRACE_INFO(tracer_, "BasicMockDbClient::rollback_transaction()");
    return IRes<>::sOk();
}
demiplane::IRes<> BasicMockDbClient::connect(const ConnectParams& params) {
    TRACE_INFO(tracer_, "BasicMockDbClient::connect()");
    return IRes<>::sOk();
}
demiplane::IRes<> BasicMockDbClient::drop_connect() {
    TRACE_INFO(tracer_, "BasicMockDbClient::drop_connect()");
    return IRes<>::sOk();
}
demiplane::IRes<> BasicMockDbClient::create_table(const query::CreateQuery& proposal) {
    TRACE_INFO(tracer_, "BasicMockDbClient::create_table()");
    return IRes<>::sOk();
}
demiplane::IRes<> BasicMockDbClient::delete_table(std::string_view table_name) {
    TRACE_INFO(tracer_, "BasicMockDbClient::delete_table()");
    return IRes<>::sOk();
}
demiplane::IRes<> BasicMockDbClient::truncate_table(std::string_view table_name) {
    TRACE_INFO(tracer_, "BasicMockDbClient::truncate_table()");
    return IRes<>::sOk();

}
demiplane::IRes<bool> BasicMockDbClient::check_table(std::string_view table_name) {
    TRACE_INFO(tracer_, "BasicMockDbClient::check_table()");
    return IRes<bool>{true};
}
demiplane::IRes<std::optional<Records>> BasicMockDbClient::insert(query::InsertQuery&& query) {
    TRACE_INFO(tracer_, "BasicMockDbClient::insert()");
    return IRes<std::optional<Records>>::sOk();
}
demiplane::IRes<std::optional<Records>> BasicMockDbClient::upsert(query::UpsertQuery&& query) {
    TRACE_INFO(tracer_, "BasicMockDbClient::upsert()");
    return IRes<std::optional<Records>>::sOk();
}

demiplane::IRes<Records> BasicMockDbClient::select(
    const query::SelectQuery& conditions) const {
    TRACE_INFO(tracer_, "BasicMockDbClient::select()");
    return demiplane::IRes<Records>{};
}
demiplane::IRes<std::optional<Records>> BasicMockDbClient::remove(const query::DeleteQuery& conditions) {
    TRACE_INFO(tracer_, "BasicMockDbClient::remove()");
    return IRes<std::optional<Records>>::sOk();
}
demiplane::IRes<uint32_t> BasicMockDbClient::count(const query::CountQuery& conditions) const {
    TRACE_INFO(tracer_, "BasicMockDbClient::count()");
    return IRes<uint32_t>{0};
}