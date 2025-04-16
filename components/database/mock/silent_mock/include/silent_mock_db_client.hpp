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
        gears::Result create_database(const std::shared_ptr<DatabaseConfig>& config, const ConnectParams& pr) override;
        gears::Result connect(const ConnectParams& params) override;
        gears::Result start_transaction() override;
        gears::Result commit_transaction() override;
        gears::Result rollback_transaction() override;
        gears::Result drop_connect() override;
        gears::Result create_table(const query::CreateTableQuery& query) override;
        gears::Result drop_table(const query::DropTableQuery& query) override;
        gears::Result truncate_table(const query::TruncateTableQuery& query) override;
        [[nodiscard]] gears::Interceptor<bool> check_table(const query::CheckTableQuery& query) override;
        gears::Interceptor<std::optional<Records>> insert(query::InsertQuery query) override;
        gears::Interceptor<std::optional<Records>> upsert(query::UpsertQuery&& query) override;
        [[nodiscard]] gears::Interceptor<Records> select(const query::SelectQuery& conditions) const override;
        gears::Interceptor<std::optional<Records>> remove(const query::RemoveQuery& conditions) override;
        [[nodiscard]] gears::Interceptor<uint32_t> count(const query::CountQuery& conditions) const override;

        static constexpr const char* name() {
            return "SilentMockDbClient";
        }

        gears::Result set_unique_constraint(const query::SetUniqueConstraint& query) override;
        gears::Result delete_unique_constraint(const query::DeleteUniqueConstraint& query) override;

    protected:
        [[nodiscard]] std::exception_ptr analyze_exception(const std::exception& caught_exception) const override;

    private:
        mutable math::random::RandomTimeGenerator generator_;
    };
} // namespace demiplane::database
