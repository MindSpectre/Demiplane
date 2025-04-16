#include "silent_mock_db_client.hpp"
using namespace demiplane::database;
SilentMockDbClient::~SilentMockDbClient() {
    std::this_thread::sleep_for(generator_.generate_milliseconds(30));
}
demiplane::Result SilentMockDbClient::create_database(
    const std::shared_ptr<DatabaseConfig>& config, const ConnectParams& pr) {
    std::this_thread::sleep_for(generator_.generate_milliseconds(200, 20));
    return Result::sOk();
}
demiplane::Result SilentMockDbClient::connect(const ConnectParams& params) {
    DbBase::connect(params);
    return Result::sOk();
}
demiplane::Result SilentMockDbClient::start_transaction() {
    std::this_thread::sleep_for(generator_.generate_milliseconds(5));
    return Result::sOk();
}
demiplane::Result SilentMockDbClient::commit_transaction() {
    std::this_thread::sleep_for(generator_.generate_milliseconds(10));
    return Result::sOk();
}
demiplane::Result SilentMockDbClient::rollback_transaction() {
    std::this_thread::sleep_for(generator_.generate_milliseconds(10));
    return Result::sOk();
}
demiplane::Result SilentMockDbClient::drop_connect() {
    std::this_thread::sleep_for(generator_.generate_milliseconds(30));
    return Result::sOk();
}
demiplane::Result SilentMockDbClient::create_table(const query::CreateTableQuery& query) {
    std::this_thread::sleep_for(generator_.generate_milliseconds(15));
    return Result::sOk();
}
demiplane::Result SilentMockDbClient::drop_table(const query::DropTableQuery& query) {
    std::this_thread::sleep_for(generator_.generate_milliseconds(15));
    return Result::sOk();
}
demiplane::Result SilentMockDbClient::truncate_table(const query::TruncateTableQuery& query) {
    std::this_thread::sleep_for(generator_.generate_milliseconds(15));
    return Result::sOk();
}
demiplane::Interceptor<bool> SilentMockDbClient::check_table(const query::CheckTableQuery& query) {
    std::this_thread::sleep_for(generator_.generate_milliseconds(15));
    return Interceptor{true};
}

demiplane::Interceptor<std::optional<Records>> SilentMockDbClient::insert(query::InsertQuery query) {
    std::this_thread::sleep_for(generator_.generate_milliseconds(100, 70));
    return Interceptor<std::optional<Records>>::sOk();
}
demiplane::Interceptor<std::optional<Records>> SilentMockDbClient::upsert(query::UpsertQuery&& query) {
    std::this_thread::sleep_for(generator_.generate_milliseconds(120, 70));
    return Interceptor<std::optional<Records>>::sOk();
}
demiplane::Interceptor<Records> SilentMockDbClient::select(const query::SelectQuery& conditions) const {
    std::this_thread::sleep_for(generator_.generate_milliseconds(250, 80));
    return demiplane::Interceptor<Records>{};
}
demiplane::Interceptor<std::optional<Records>> SilentMockDbClient::remove(const query::RemoveQuery& conditions) {
    std::this_thread::sleep_for(generator_.generate_milliseconds(40));
    return Interceptor<std::optional<Records>>::sOk();
}
demiplane::Interceptor<unsigned> SilentMockDbClient::count(const query::CountQuery& conditions) const {
    std::this_thread::sleep_for(generator_.generate_milliseconds(40));
    return Interceptor<unsigned>{0};
}
demiplane::Result SilentMockDbClient::set_unique_constraint(const query::SetUniqueConstraint& query) {
    std::this_thread::sleep_for(generator_.generate_milliseconds(30));
    return Result::sOk();
}
demiplane::Result SilentMockDbClient::delete_unique_constraint(const query::DeleteUniqueConstraint& query) {
    std::this_thread::sleep_for(generator_.generate_milliseconds(30));
    return Result::sOk();
}

std::exception_ptr SilentMockDbClient::analyze_exception(const std::exception& caught_exception) const {
    return std::make_exception_ptr(caught_exception);
}
