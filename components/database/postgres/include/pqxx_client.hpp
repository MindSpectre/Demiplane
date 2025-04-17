#pragma once

#include <memory>
#include <mutex>
#include <pqxx/pqxx>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <vector>

#include "../pqxx_configurator.hpp"
#include "db_base.hpp"
#include "traits/search_trait.hpp"
#include "traits/table_management_trait.hpp"
#include "traits/transaction_trait.hpp"
#include "traits/unique_constraint_trait.hpp"
#include <boost/container/flat_map.hpp>

namespace demiplane::database {

    class PqxxClient final : public DbBase<PqxxClient>,
                             public TransactionTrait,
                             public UniqueConstraintTrait,
                             public TableTrait,
                             public SearchTrait {
    public:
        ~PqxxClient() override = default;
        PqxxClient(const ConnectParams& connect_params, std::shared_ptr<scroll::Tracer<PqxxClient>> tracer);

        gears::Result create_database(const std::shared_ptr<DatabaseConfig>& config, const ConnectParams& pr) override;
        gears::Result start_transaction() override;
        gears::Result commit_transaction() override;
        gears::Result rollback_transaction() override;
        gears::Result connect(const ConnectParams& params) override;
        gears::Result drop_connect() override;
        gears::Result create_table(const query::CreateTableQuery& query) override;
        gears::Result drop_table(const query::DropTableQuery& query) override;
        gears::Result truncate_table(const query::TruncateTableQuery& query) override;
        [[nodiscard]] gears::Interceptor<bool> check_table(const query::CheckTableQuery& query) override;
        gears::Result set_unique_constraint(const query::SetUniqueConstraint& query) override;
        gears::Result delete_unique_constraint(const query::DeleteUniqueConstraint& table_name) override;
        gears::Interceptor<std::optional<Records>> insert(query::InsertQuery query) override;
        gears::Interceptor<std::optional<Records>> upsert(query::UpsertQuery&& query) override;
        [[nodiscard]] gears::Interceptor<Records> select(const query::SelectQuery& query) const override;
        gears::Interceptor<std::optional<Records>> remove(const query::RemoveQuery& query) override;
        [[nodiscard]] gears::Interceptor<uint32_t> count(const query::CountQuery& query) const override;
        static constexpr const char* name() {
            return "Postgres client";
        }
        [[nodiscard]] gears::Result setup_search_index(const query::SetIndexQuery& query) override;
        [[nodiscard]] gears::Result drop_search_index(const query::DropIndexQuery& query) override;

    private:
        PostgresConfig configuration_;
        // Cached type OIDs and fields used in conflict/upsert and search operations
        boost::container::flat_map<uint32_t, std::string> type_oids_; // id -> type name

        std::shared_ptr<pqxx::connection> conn_;
        mutable std::mutex conn_mutex_;
        mutable std::shared_mutex type_oid_mutex_;
        mutable std::unique_ptr<pqxx::work> open_transaction_;
        bool in_transaction_;

        // Helper functions:
        gears::Result oid_preprocess();
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
