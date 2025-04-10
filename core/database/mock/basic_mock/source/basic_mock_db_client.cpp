#include "basic_mock_db_client.hpp"
#include "ires.hpp"
using namespace demiplane::database;
#define ENABLE_TRACING

BasicMockDbClient::BasicMockDbClient() {
    i_tracer = scroll::TracerFactory::create_default_console_tracer<BasicMockDbClient>();
    TRACE_INFO(i_tracer, "BasicMockDbClient has been created.");
}
BasicMockDbClient::~BasicMockDbClient() {
    TRACE_INFO(i_tracer, "BasicMockDbClient has been destructed.");
}
demiplane::Result BasicMockDbClient::create_database(
    const std::shared_ptr<DatabaseConfig>& config, const ConnectParams& pr) {
    TRACE_INFO(i_tracer, "BasicMockDbClient::create_database()");
    return Result::sOk();
}

demiplane::Result BasicMockDbClient::start_transaction() {
    TRACE_INFO(i_tracer, "BasicMockDbClient::start_transaction()");
    return Result::sOk();
}
demiplane::Result BasicMockDbClient::commit_transaction() {
    TRACE_INFO(i_tracer, "BasicMockDbClient::commit_transaction()");
    return Result::sOk();
}
demiplane::Result BasicMockDbClient::rollback_transaction() {
    TRACE_INFO(i_tracer, "BasicMockDbClient::rollback_transaction()");
    return Result::sOk();
}
demiplane::Result BasicMockDbClient::connect(const ConnectParams& params) {
    TRACE_INFO(i_tracer, "BasicMockDbClient::connect()");
    return Result::sOk();
}
demiplane::Result BasicMockDbClient::drop_connect() {
    TRACE_INFO(i_tracer, "BasicMockDbClient::drop_connect()");
    return Result::sOk();
}
demiplane::Result BasicMockDbClient::create_table(const query::CreateTableQuery& proposal) {
    TRACE_INFO(i_tracer, "BasicMockDbClient::create_table()");
    return Result::sOk();
}
demiplane::Result BasicMockDbClient::drop_table(const query::DropTableQuery& table_name) {
    TRACE_INFO(i_tracer, "BasicMockDbClient::delete_table()");
    return Result::sOk();
}
demiplane::Result BasicMockDbClient::truncate_table(const query::TruncateTableQuery& table_name) {
    TRACE_INFO(i_tracer, "BasicMockDbClient::truncate_table()");
    return Result::sOk();

}
demiplane::Interceptor<bool> BasicMockDbClient::check_table(const query::CheckTableQuery& table_name) {
    TRACE_INFO(i_tracer, "BasicMockDbClient::check_table()");
    return Interceptor<bool>{true};
}
demiplane::Interceptor<std::optional<Records>> BasicMockDbClient::insert(query::InsertQuery&& query) {
    TRACE_INFO(i_tracer, "BasicMockDbClient::insert()");
    return Interceptor<std::optional<Records>>::sOk();
}
demiplane::Interceptor<std::optional<Records>> BasicMockDbClient::upsert(query::UpsertQuery&& query) {
    TRACE_INFO(i_tracer, "BasicMockDbClient::upsert()");
    return Interceptor<std::optional<Records>>::sOk();
}

demiplane::Interceptor<Records> BasicMockDbClient::select(
    const query::SelectQuery& conditions) const {
    TRACE_INFO(i_tracer, "BasicMockDbClient::select()");
    return demiplane::Interceptor<Records>{};
}
demiplane::Interceptor<std::optional<Records>> BasicMockDbClient::remove(const query::RemoveQuery& conditions) {
    TRACE_INFO(i_tracer, "BasicMockDbClient::remove()");
    return Interceptor<std::optional<Records>>::sOk();
}
demiplane::Interceptor<uint32_t> BasicMockDbClient::count(const query::CountQuery& conditions) const {
    TRACE_INFO(i_tracer, "BasicMockDbClient::count()");
    return Interceptor<uint32_t>{0};
}
demiplane::Result BasicMockDbClient::set_unique_constraint(const query::SetUniqueConstraint& query) {
    TRACE_INFO(i_tracer, "BasicMockDbClient::make_unique_constraint()");
    return Result::sOk();
}
demiplane::Result BasicMockDbClient::delete_unique_constraint(const query::DeleteUniqueConstraint& table_name) {
    TRACE_INFO(i_tracer, "BasicMockDbClient::delete_unique_constraint()");
    return Result::sOk();
}
std::exception_ptr BasicMockDbClient::analyze_exception(const std::exception& caught_exception) const {
    return std::make_exception_ptr(caught_exception);
}
