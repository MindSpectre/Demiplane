#pragma once

#include <chrono>
#include <demiplane/math>
#include <memory>
#include <vector>

#include "db_base.hpp"
#include "traits/search_trait.hpp"
#include "traits/table_management_trait.hpp"
#include "traits/transaction_trait.hpp"
#include "traits/unique_constraint_trait.hpp"
namespace demiplane::database {
    class SilentMockDbClient final : public DbBase<SilentMockDbClient>,
                                     public TransactionTrait,
                                     public TableTrait,
                                     public UniqueConstraintTrait {
    public:
        ~SilentMockDbClient() override;
        Result create_database(const std::shared_ptr<DatabaseConfig>& config, const ConnectParams& pr) override;
        Result connect(const ConnectParams& params) override;
        Result start_transaction() override;
        Result commit_transaction() override;
        Result rollback_transaction() override;
        Result drop_connect() override;
        Result create_table(const query::CreateTableQuery& query) override;
        Result drop_table(const query::DropTableQuery& query) override;
        Result truncate_table(const query::TruncateTableQuery& query) override;
        [[nodiscard]] Interceptor<bool> check_table(const query::CheckTableQuery& query) override;
        Interceptor<std::optional<Records>> insert(query::InsertQuery query) override;
        Interceptor<std::optional<Records>> upsert(query::UpsertQuery&& query) override;
        [[nodiscard]] Interceptor<Records> select(const query::SelectQuery& conditions) const override;
        Interceptor<std::optional<Records>> remove(const query::RemoveQuery& conditions) override;
        [[nodiscard]] Interceptor<uint32_t> count(const query::CountQuery& conditions) const override;

        static constexpr const char* name() {
            return "SilentMockDbClient";
        }

        Result set_unique_constraint(const query::SetUniqueConstraint& query) override;
        Result delete_unique_constraint(const query::DeleteUniqueConstraint& query) override;

    protected:
        [[nodiscard]] std::exception_ptr analyze_exception(const std::exception& caught_exception) const override;

    private:
        mutable math::random::RandomTimeGenerator generator_;
    };
} // namespace demiplane::database
