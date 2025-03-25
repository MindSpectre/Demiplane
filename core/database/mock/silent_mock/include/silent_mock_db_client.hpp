#pragma once

#include <chrono>
#include <iostream>
#include <memory>
#include <regex>
#include <string_view>
#include <thread>
#include <vector>

#include "chrono_utils.hpp"
#include "db_interface.hpp"

namespace demiplane::database {
    class SilentMockDbClient final : public DbInterface,
                                     public TransactionTrait,
                                     public TableTrait,
                                     HasName<SilentMockDbClient> {
    public:
        ~SilentMockDbClient() override;
        IRes<> create_database(const std::shared_ptr<DatabaseConfig>& config, const ConnectParams& pr) override;
        IRes<> connect(const ConnectParams& params) override;
        IRes<> start_transaction() override;
        IRes<> commit_transaction() override;
        IRes<> rollback_transaction() override;
        IRes<> drop_connect() override;
        IRes<> create_table(const query::CreateQuery& proposal) override;
        IRes<> delete_table(std::string_view table_name) override;
        IRes<> truncate_table(std::string_view table_name) override;
        [[nodiscard]] IRes<bool> check_table(std::string_view table_name) override;
        IRes<std::optional<Records>> insert(query::InsertQuery&& query) override;
        IRes<std::optional<Records>> upsert(query::UpsertQuery&& query) override;
        [[nodiscard]] IRes<Records> select(const query::SelectQuery& conditions) const override;
        IRes<std::optional<Records>> remove(const query::DeleteQuery& conditions) override;
        [[nodiscard]] IRes<uint32_t> count(const query::CountQuery& conditions) const override;

        static constexpr const char* name() {
            return "SilentMockDbClient";
        }

    };
} // namespace demiplane::database
