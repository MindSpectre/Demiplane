// pqxx_client.hpp
#pragma once

#include <demiplane/scroll>
#include <memory>
#include <vector>

#include "db_base.hpp"
#include "traits/table_management_trait.hpp"
#include "traits/transaction_trait.hpp"
#include "traits/unique_constraint_trait.hpp"


namespace demiplane::database {
    class BasicMockDbClient final : public DbBase<BasicMockDbClient>,
                                    public TransactionTrait,
                                    public TableTrait,
                                    public UniqueConstraintTrait {
    public:
        BasicMockDbClient();
        ~BasicMockDbClient() override;

        BasicMockDbClient(const ConnectParams& params, std::shared_ptr<scroll::Tracer<BasicMockDbClient>> tracer)
            : DbBase(params, std::move(tracer)) {}
        Result create_database(const std::shared_ptr<DatabaseConfig>& config, const ConnectParams& pr) override;

        Result start_transaction() override;

        Result commit_transaction() override;

        Result rollback_transaction() override;

        Result connect(const ConnectParams& params) override;

        Result drop_connect() override;

        Result create_table(const query::CreateTableQuery& proposal) override;

        Result drop_table(const query::DropTableQuery& table_name) override;

        Result truncate_table(const query::TruncateTableQuery& table_name) override;

        [[nodiscard]] Interceptor<bool> check_table(const query::CheckTableQuery& table_name) override;

        Interceptor<std::optional<Records>> insert(query::InsertQuery query) override;

        Interceptor<std::optional<Records>> upsert(query::UpsertQuery&& query) override;


        [[nodiscard]] Interceptor<Records> select(const query::SelectQuery& conditions) const override;

        Interceptor<std::optional<Records>> remove(const query::RemoveQuery& conditions) override;

        [[nodiscard]] Interceptor<uint32_t> count(const query::CountQuery& conditions) const override;


        static constexpr const char* name() {
            return "BASIC_MOCK_DB_CLIENT";
        }
        Result set_unique_constraint(const query::SetUniqueConstraint& query) override;
        Result delete_unique_constraint(const query::DeleteUniqueConstraint& table_name) override;

    protected:
        [[nodiscard]] std::exception_ptr analyze_exception(const std::exception& caught_exception) const override;
    };
} // namespace demiplane::database
