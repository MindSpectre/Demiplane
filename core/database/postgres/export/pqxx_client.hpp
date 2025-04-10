#pragma once

#include <memory>
#include <mutex>
#include <pqxx/pqxx>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <vector>

#include <boost/container/flat_map.hpp>

#include "db_interface.hpp"
#include "traits/search_trait.hpp"
#include "traits/table_management_trait.hpp"
#include "traits/transaction_trait.hpp"
#include "traits/unique_constraint_trait.hpp"

#include "../include/pqxx_configurator.hpp"
namespace demiplane::database {

    class PqxxClient final : public DbInterface<PqxxClient>,
                             public TransactionTrait,
                             public UniqueConstraintTrait,
                             public TableTrait,
                             public SearchTrait,
                             public HasName<PqxxClient> {
    public:
        ~PqxxClient() override = default;
        explicit PqxxClient(const ConnectParams& pr);
        PqxxClient(const ConnectParams& connect_params, std::unique_ptr<scroll::TracerInterface<PqxxClient>> tracer)
            : DbInterface(connect_params, std::move(tracer)), in_transaction_(false) {}

        Result create_database(const std::shared_ptr<DatabaseConfig>& config, const ConnectParams& pr) override;
        Result start_transaction() override;
        Result commit_transaction() override;
        Result rollback_transaction() override;
        Result connect(const ConnectParams& params) override;
        Result drop_connect() override;
        Result create_table(const query::CreateTableQuery& query) override;
        Result drop_table(const query::DropTableQuery&  query) override;
        Result truncate_table(const query::TruncateTableQuery&  query) override;
        [[nodiscard]] Interceptor<bool> check_table(const query::CheckTableQuery &query) override;
        Result set_unique_constraint(const query::SetUniqueConstraint& query) override;
        Result delete_unique_constraint(const query::DeleteUniqueConstraint &table_name) override;
        Interceptor<std::optional<Records>> insert(query::InsertQuery&& query) override;
        Interceptor<std::optional<Records>> upsert(query::UpsertQuery&& query) override;
        [[nodiscard]] Interceptor<Records> select(const query::SelectQuery& query) const override;
        Interceptor<std::optional<Records>> remove(const query::RemoveQuery& query) override;
        [[nodiscard]] Interceptor<uint32_t> count(const query::CountQuery& query) const override;
        static constexpr const char* name() {
            return "Postgres client";
        }
        [[nodiscard]] Result setup_search_index(const query::SetIndexQuery& query)  override;
        [[nodiscard]] Result drop_search_index(const query::DropIndexQuery &query) override;

    private:
        PostgresConfig configuration_;
        // Cached type OIDs and fields used in conflict/upsert and search operations
        boost::container::flat_map<uint32_t, std::string> type_oids_; // id -> type name

        std::shared_ptr<pqxx::connection> conn_;
        mutable std::mutex conn_mutex_ ;
        mutable std::shared_mutex type_oid_mutex_;
        mutable std::unique_ptr<pqxx::work> open_transaction_;
        bool in_transaction_;

        // Helper functions:
        Result oid_preprocess();
        [[nodiscard]] std::unique_ptr<FieldBase> process_field(const pqxx::field& field) const;

        // Execution helpers (for non-parameterized queries and with parameters)
        void execute_query(std::string_view query_string) const;
        void execute_query(std::string_view query_string, pqxx::params&& params) const;
        pqxx::result execute_query_with_result(std::string_view query_string, pqxx::params&& params) const;
        pqxx::result execute_query_with_result(std::string_view query_string) const;

        // Transaction helpers
        std::unique_ptr<pqxx::work> initialize_transaction() const;
        void finish_transaction(std::unique_ptr<pqxx::work>&& current_transaction) const;

        std::exception_ptr analyze_exception(const std::exception& caught_exception) const override;
    };

} // namespace demiplane::database
