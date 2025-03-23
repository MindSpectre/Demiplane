// pqxx_client.hpp
#pragma once

#include <iostream>
#include <memory>
#include <regex>
#include <string_view>
#include <vector>

#include "db_interface.hpp"
#include "scroll_tracer.hpp"


namespace demiplane::database {
    class BasicMockDbClient final : public DbInterface, HasName<BasicMockDbClient> {
    public:
        BasicMockDbClient();
        ~BasicMockDbClient() override;

        explicit BasicMockDbClient(std::shared_ptr<scroll::TracerInterface> tracer) : tracer_(std::move(tracer)) {}

        BasicMockDbClient(const ConnectParams& params, std::shared_ptr<scroll::TracerInterface> tracer)
            : DbInterface(params), tracer_(std::move(tracer)) {}

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

        IRes<> setup_search_index(std::string_view table_name, FieldCollection fields) override;

        IRes<> drop_search_index(std::string_view table_name) const override;

        IRes<> remove_search_index(std::string_view table_name) override;

        IRes<> restore_search_index(std::string_view table_name) const override;

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
            return "BASIC_MOCK_DB_CLIENT";
        }

    private:
        std::shared_ptr<scroll::TracerInterface> tracer_;
    };
} // namespace demiplane::database
