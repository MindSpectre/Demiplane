// pqxx_client.hpp
#pragma once

#include <iostream>
#include <memory>
#include <regex>
#include <string_view>
#include <vector>

#include "db_interface.hpp"
#include "tracer_factory.hpp"




namespace demiplane::database {
    class BasicMockDbClient final : public DbInterface, HasName<BasicMockDbClient> {
    public:
        ~BasicMockDbClient() override;

        explicit BasicMockDbClient(std::shared_ptr<scroll::TracerInterface> tracer) : tracer_(std::move(tracer)) {}

        BasicMockDbClient(const ConnectParams& params, std::shared_ptr<scroll::TracerInterface> tracer)
            : DbInterface(params), tracer_(std::move(tracer)) {}

        void create_database(std::string_view host, uint32_t port, std::string_view db_name,
            std::string_view login, std::string_view password);

        static void create_database(const ConnectParams& pr);

        void start_transaction() override;

        void commit_transaction() override;

        void rollback_transaction() override;

        void connect(const ConnectParams& params) override;

        void drop_connect() override;

        void create_table(const query::CreateQuery& proposal) override;

        void delete_table(std::string_view table_name) override;

        void truncate_table(std::string_view table_name) override;

        [[nodiscard]] bool check_table(std::string_view table_name) override;

        void make_unique_constraint(std::string_view table_name, FieldCollection key_fields) override;

        void setup_search_index(std::string_view table_name, FieldCollection fields) override;

        void drop_search_index(std::string_view table_name) const override;

        void remove_search_index(std::string_view table_name) override;

        void restore_search_index(std::string_view table_name) const override;

        void insert(query::InsertQuery&& query) override;

        void upsert(query::UpsertQuery&& query) override;

        Records insert_with_returning(query::InsertQuery&& query) override;

        Records upsert_with_returning(query::UpsertQuery&& query) override;

        [[nodiscard]] Records select(const query::SelectQuery& conditions) const override;

        void remove(const query::DeleteQuery& conditions) override;

        [[nodiscard]] uint32_t count(const query::CountQuery& conditions) const override;

        void set_search_fields(std::string_view table_name, FieldCollection fields) override;

        void set_conflict_fields(std::string_view table_name, FieldCollection fields) override;

        static constexpr const char* name() {
            return "BASIC_MOCK_DB_CLIENT";
        }

    private:
        std::shared_ptr<scroll::TracerInterface> tracer_;
    };
} // namespace demiplane::database
