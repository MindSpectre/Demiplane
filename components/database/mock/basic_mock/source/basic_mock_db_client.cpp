#include "basic_mock_db_client.hpp"
using namespace demiplane::database;
#define ENABLE_TRACING
#include <demiplane/trace_macros>

BasicMockDbClient::BasicMockDbClient() {
    set_tracer(scroll::TracerFactory::create_default_console_tracer<BasicMockDbClient>());
    TRACE_INFO(get_tracer(), "BasicMockDbClient has been created.");
}
BasicMockDbClient::~BasicMockDbClient() {
    TRACE_INFO(get_tracer(), "BasicMockDbClient has been destructed.");
}
demiplane::gears::Result BasicMockDbClient::create_database(
    const std::shared_ptr<DatabaseConfig>& config, const ConnectParams& pr) {
    TRACE_INFO(get_tracer(), "BasicMockDbClient::create_database()");
    return gears::Result::sOk();
}

demiplane::gears::Result BasicMockDbClient::start_transaction() {
    TRACE_INFO(get_tracer(), "BasicMockDbClient::start_transaction()");
    return gears::Result::sOk();
}
demiplane::gears::Result BasicMockDbClient::commit_transaction() {
    TRACE_INFO(get_tracer(), "BasicMockDbClient::commit_transaction()");
    return gears::Result::sOk();
}
demiplane::gears::Result BasicMockDbClient::rollback_transaction() {
    TRACE_INFO(get_tracer(), "BasicMockDbClient::rollback_transaction()");
    return gears::Result::sOk();
}
demiplane::gears::Result BasicMockDbClient::connect(const ConnectParams& params) {
    TRACE_INFO(get_tracer(), "BasicMockDbClient::connect()");
    return gears::Result::sOk();
}
demiplane::gears::Result BasicMockDbClient::drop_connect() {
    TRACE_INFO(get_tracer(), "BasicMockDbClient::drop_connect()");
    return gears::Result::sOk();
}
demiplane::gears::Result BasicMockDbClient::create_table(const query::CreateTableQuery& proposal) {
    TRACE_INFO(get_tracer(), "BasicMockDbClient::create_table()");
    return gears::Result::sOk();
}
demiplane::gears::Result BasicMockDbClient::drop_table(const query::DropTableQuery& table_name) {
    TRACE_INFO(get_tracer(), "BasicMockDbClient::delete_table()");
    return gears::Result::sOk();
}
demiplane::gears::Result BasicMockDbClient::truncate_table(const query::TruncateTableQuery& table_name) {
    TRACE_INFO(get_tracer(), "BasicMockDbClient::truncate_table()");
    return gears::Result::sOk();
}
demiplane::gears::Interceptor<bool> BasicMockDbClient::check_table(const query::CheckTableQuery& table_name) {
    TRACE_INFO(get_tracer(), "BasicMockDbClient::check_table()");
    return gears::Interceptor<bool>{true};
}
demiplane::gears::Interceptor<std::optional<Records>> BasicMockDbClient::insert(query::InsertQuery query) {
    TRACE_INFO(get_tracer(), "BasicMockDbClient::insert()");
    return gears::Interceptor<std::optional<Records>>::sOk();
}
demiplane::gears::Interceptor<std::optional<Records>> BasicMockDbClient::upsert(query::UpsertQuery&& query) {
    TRACE_INFO(get_tracer(), "BasicMockDbClient::upsert()");
    return gears::Interceptor<std::optional<Records>>::sOk();
}

demiplane::gears::Interceptor<Records> BasicMockDbClient::select(const query::SelectQuery& conditions) const {
    TRACE_INFO(get_tracer(), "BasicMockDbClient::select()");
    return demiplane::gears::Interceptor<Records>{};
}
demiplane::gears::Interceptor<std::optional<Records>> BasicMockDbClient::remove(const query::RemoveQuery& conditions) {
    TRACE_INFO(get_tracer(), "BasicMockDbClient::remove()");
    return gears::Interceptor<std::optional<Records>>::sOk();
}
demiplane::gears::Interceptor<uint32_t> BasicMockDbClient::count(const query::CountQuery& conditions) const {
    TRACE_INFO(get_tracer(), "BasicMockDbClient::count()");
    return gears::Interceptor<uint32_t>{0};
}
demiplane::gears::Result BasicMockDbClient::set_unique_constraint(const query::SetUniqueConstraint& query) {
    TRACE_INFO(get_tracer(), "BasicMockDbClient::make_unique_constraint()");
    return gears::Result::sOk();
}
demiplane::gears::Result BasicMockDbClient::delete_unique_constraint(const query::DeleteUniqueConstraint& table_name) {
    TRACE_INFO(get_tracer(), "BasicMockDbClient::delete_unique_constraint()");
    return gears::Result::sOk();
}
std::exception_ptr BasicMockDbClient::analyze_exception(const std::exception& caught_exception) const {
    return std::make_exception_ptr(caught_exception);
}
