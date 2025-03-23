#pragma once

#include <memory>
#include <mutex>
#include <pqxx/pqxx>
#include <string>
#include <string_view>
#include <vector>
#include <sstream>
#include <regex>
#include "boost/container/flat_map.hpp"

#include "db_connect_params.hpp"
#include "db_exceptions.hpp"
#include "db_interface.hpp"
#include "pqxx_qeury_processor.hpp" // Contains PqxxQueryProcessor and QueryAndParams

namespace demiplane::database {

class PqxxClient final : public DbInterface, public HasName<PqxxClient> {
public:
    ~PqxxClient() override = default;
    explicit PqxxClient(const ConnectParams& pr);
    PqxxClient(const ConnectParams& connect_params, std::unique_ptr<scroll::TracerInterface> tracer)
        : DbInterface(connect_params, std::move(tracer)), in_transaction_(false) {}

    IRes<> create_database(const std::shared_ptr<DatabaseConfig>& config, const ConnectParams& pr) override;
    IRes<> start_transaction() override;
    IRes<> commit_transaction() override;
    IRes<> rollback_transaction() override;
    IRes<> connect(const ConnectParams& params) override;
    IRes<> drop_connect() override;
    IRes<> create_table(const query::CreateQuery& proposal) override;
    IRes<> delete_table(std::string_view table_name) override;
    IRes<> truncate_table(std::string_view table_name) override;
    [[nodiscard]] IRes<bool> check_table(std::string_view table_name) override;
    IRes<> make_unique_constraint(std::string_view table_name, FieldCollection key_fields) override;
    [[nodiscard]] IRes<> setup_search_index(std::string_view table_name, FieldCollection fields) override;
    [[nodiscard]] IRes<> drop_search_index(std::string_view table_name) const override;
    [[nodiscard]] IRes<> remove_search_index(std::string_view table_name) override;
    [[nodiscard]] IRes<> restore_search_index(std::string_view table_name) const override;
    IRes<> insert(query::InsertQuery&& query) override;
    IRes<> upsert(query::UpsertQuery&& query) override;
    IRes<Records> insert_with_returning(query::InsertQuery&& query) override;
    IRes<Records> upsert_with_returning(query::UpsertQuery&& query) override;
    [[nodiscard]] IRes<Records> select(const query::SelectQuery& conditions) const override;
    IRes<> remove(const query::DeleteQuery& conditions) override;
    [[nodiscard]] IRes<uint32_t> count(const query::CountQuery& conditions) const override;
    void set_search_fields(std::string_view table_name, FieldCollection fields) noexcept override;
    void set_conflict_fields(std::string_view table_name, FieldCollection fields) noexcept override;

    static constexpr const char* name() {
        return "Postgres client";
    }

private:
    // Cached type OIDs and fields used in conflict/upsert and search operations
    boost::container::flat_map<uint32_t, std::string> type_oids_; // id -> type name
    boost::container::flat_map<std::string, FieldCollection> conflict_fields_;
    boost::container::flat_map<std::string, FieldCollection> search_fields_;

    std::shared_ptr<pqxx::connection> conn_;
    mutable std::recursive_mutex conn_mutex_;
    mutable std::unique_ptr<pqxx::work> open_transaction_;
    bool in_transaction_;

    // Helper functions:
    IRes<> oid_preprocess();
    [[nodiscard]] std::unique_ptr<FieldBase> process_field(const pqxx::field& field) const;
    [[nodiscard]] static bool is_valid_identifier(std::string_view identifier);
    std::string escape_identifier(std::string_view identifier) const;

    // Execution helpers (for non-parameterized queries and with parameters)
    IRes<> execute_query(const std::string& query_string, const pqxx::params& params) const;
    IRes<> execute_query(const std::string& query_string) const;
    IRes<pqxx::result> execute_query_with_result(const std::string& query_string, const pqxx::params& params) const;
    IRes<pqxx::result> execute_query_with_result(const std::string& query_string) const;

    // Transaction helpers
    std::unique_ptr<pqxx::work> initialize_transaction() const;
    void finish_transaction(std::unique_ptr<pqxx::work>&& current_transaction) const;
    static exceptions::DatabaseException adapt_exception(const std::exception& pqxx_exception);

    // Conflict/Returning clause builders
    void build_conflict_clause_for_force_insert(std::string& query, std::string_view table_name, const FieldCollection& replace_fields) const;
    void build_returning_clause(std::string& query, const FieldCollection& returning_fields);

    // FTS / Trigram helpers (and extension installation)
    void create_fts_index_query(std::string_view table_name, std::ostringstream& index_query) const;
    void create_trgm_index_query(std::string_view table_name, std::ostringstream& index_query) const;
    static std::string make_fts_index_name(std::string_view table_name);
    static std::string make_trgm_index_name(std::string_view table_name);
    void install_trgm_extension() const;
};

} // namespace demiplane::database
