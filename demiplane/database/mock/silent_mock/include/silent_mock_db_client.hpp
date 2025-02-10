// pqxx_client.hpp
#pragma once

#include <chrono>
#include <iostream>
#include <memory>
#include <regex>
#include <string_view>
#include <thread>
#include <vector>

#include "chrono_utils.hpp"
#include "db_connect_params.hpp"
#include "db_interface.hpp"

namespace common::database {
    class SilentMockDbClient final : public interfaces::DbInterface {
    public:
        /// @brief Creating database with given params using template db
        static void create_database(const ConnectParams& params) {
            std::this_thread::sleep_for(utilities::chrono::RandomTimeGenerator::generate(100));
        }

        /// @brief Explicitly close the connection. Destructor do the same, but this function can throw exception
        void drop_connect() override {
            std::this_thread::sleep_for(utilities::chrono::RandomTimeGenerator::generate(10));
        }


        /// @brief Trying to connect to the database, if connection is not open will throw exception
        /// @throws common::database::exceptions::ConnectionException
        explicit SilentMockDbClient(const ConnectParams& params) {
            std::this_thread::sleep_for(utilities::chrono::RandomTimeGenerator::generate(10));
        }

        SilentMockDbClient() {
            std::this_thread::sleep_for(utilities::chrono::RandomTimeGenerator::generate(10));
        }
        ~SilentMockDbClient() override = default;

        // Transaction Methods
        /// @brief Start a transaction. All queries before commiting/roll backing a transaction will use common
        /// transaction
        /// @throws  common::database::exceptions::TransactionException
        void start_transaction() override {
            std::this_thread::sleep_for(utilities::chrono::RandomTimeGenerator::generate(10));
        }

        /// @brief Commit a transaction. If throws an exception, all transaction will be reverted
        /// @throws  common::database::exceptions::TransactionException
        void commit_transaction() override {
            std::this_thread::sleep_for(utilities::chrono::RandomTimeGenerator::generate(10));
        }

        /// @brief Instantly cancel a transaction
        /// @throws  common::database::exceptions::TransactionException
        void rollback_transaction() override {
            std::this_thread::sleep_for(utilities::chrono::RandomTimeGenerator::generate(10));
        }

        /// @brief Create unique index for the table
        /// @param table_name For which table created index.
        /// @param conflict_fields Fields which will be unique for each record
        void make_unique_constraint(
            std::string_view table_name, std::vector<std::shared_ptr<FieldBase>> conflict_fields) override {
            std::this_thread::sleep_for(utilities::chrono::RandomTimeGenerator::generate(50));
        }

        /// @brief Create full test search index for given fields
        /// @param table_name For which table created index.
        /// @param fields Fts fields
        void setup_search_index(std::string_view table_name, std::vector<std::shared_ptr<FieldBase>> fields) override {
            std::this_thread::sleep_for(utilities::chrono::RandomTimeGenerator::generate(150));
        }

        /// @brief Drop index, but doesn't remove fts fields from this client. Allows to restore it(reindex) using
        /// restore_full_text_search method
        void drop_search_index(std::string_view table_name) const override {
            std::this_thread::sleep_for(utilities::chrono::RandomTimeGenerator::generate(50));
        }

        /// @brief Drop index + remove fields from this client. For using fts further setup_fulltext_search should be
        /// called again
        void remove_search_index(std::string_view table_name) override {
            std::this_thread::sleep_for(utilities::chrono::RandomTimeGenerator::generate(50));
        }

        /// @brief Restore index + reindex. Use previous declared fts fields
        void restore_search_index(std::string_view table_name) const override {
            std::this_thread::sleep_for(utilities::chrono::RandomTimeGenerator::generate(150));
        }

        // Table Management

        /// @param table_name new table name
        /// @param field_list properties of fields of new table
        void create_table(std::string_view table_name, const Record& field_list) override {
            std::this_thread::sleep_for(utilities::chrono::RandomTimeGenerator::generate(30));
        }

        void remove_table(std::string_view table_name) override {
            std::this_thread::sleep_for(utilities::chrono::RandomTimeGenerator::generate(30));
        }

        void truncate_table(std::string_view table_name) override {
            std::this_thread::sleep_for(utilities::chrono::RandomTimeGenerator::generate(20));
        }

        /// @return Existence status of table
        [[nodiscard]] bool check_table(std::string_view table_name) override {
            std::this_thread::sleep_for(utilities::chrono::RandomTimeGenerator::generate(25));
            return true;
        }

        // Data Retrieval

        /// @brief Select records follow conditions. Copy all data to own it
        /// @return Vector of records. Each element of vector - one row in a table
        [[nodiscard]] std::vector<Record> select(
            std::string_view table_name, const Conditions& conditions) const override {
            std::this_thread::sleep_for(utilities::chrono::RandomTimeGenerator::generate(60, 70));
            return {};
        }

        /// @brief Select all table records.
        /// @warning Slow, because of copying all fields
        [[nodiscard]] std::vector<Record> select(std::string_view table_name) const override {
            std::this_thread::sleep_for(utilities::chrono::RandomTimeGenerator::generate(200, 90));
            return {};
        }

        /// @brief Faster than select, but doesn't transform and allows only one operation view field
        /// @return Vector of view records. Each element of vector - one row in a table
        /// @warning If u need not only view data, use select.
        [[nodiscard]] std::vector<std::unique_ptr<ViewRecord>> view(
            std::string_view table_name, const Conditions& conditions) const override {
            std::this_thread::sleep_for(utilities::chrono::RandomTimeGenerator::generate(40, 70));
            return {};
        }

        /// @brief Faster than select, but doesn't transform and allows only one operation view field
        /// @return Vector of view records. Each element of vector - one row in a table
        /// @warning If u need not only view data, use select.
        [[nodiscard]] std::vector<std::unique_ptr<ViewRecord>> view(std::string_view table_name) const override {
            std::this_thread::sleep_for(utilities::chrono::RandomTimeGenerator::generate(120, 40));
            return {};
        }

        // Remove Data
        ///@brief remove data following conditions
        void remove(std::string_view table_name, const Conditions& conditions) override {
            std::this_thread::sleep_for(utilities::chrono::RandomTimeGenerator::generate(250, 90));
        }

        // Get Record Count
        /// @return Count of records
        [[nodiscard]] uint32_t count(std::string_view table_name, const Conditions& conditions) const override {
            std::this_thread::sleep_for(utilities::chrono::RandomTimeGenerator::generate(80, 70));
            return 0;
        }

        /// @return Count of all records in table
        [[nodiscard]] uint32_t count(std::string_view table_name) const override {
            std::this_thread::sleep_for(utilities::chrono::RandomTimeGenerator::generate(50, 5));
            return 0;
        }

        void set_search_fields(std::string_view table_name, std::vector<std::shared_ptr<FieldBase>> fields) override {
            std::this_thread::sleep_for(utilities::chrono::RandomTimeGenerator::generate(10));
        }

        void set_conflict_fields(std::string_view table_name, std::vector<std::shared_ptr<FieldBase>> fields) override {
            std::this_thread::sleep_for(utilities::chrono::RandomTimeGenerator::generate(10));
        }

    protected:
        std::vector<Record> insert_with_returning_implementation(std::string_view table_name,
            const std::vector<Record>& rows, const std::vector<std::shared_ptr<FieldBase>>& returning_fields) override {
            return {};
        }

        std::vector<Record> insert_with_returning_implementation(std::string_view table_name,
            std::vector<Record>&& rows, const std::vector<std::shared_ptr<FieldBase>>& returning_fields) override {
            return {};
        }

        std::vector<Record> upsert_with_returning_implementation(std::string_view table_name,
            const std::vector<Record>& rows, const std::vector<std::shared_ptr<FieldBase>>& replace_fields,
            const std::vector<std::shared_ptr<FieldBase>>& returning_fields) override {
            return {};
        }

        std::vector<Record> upsert_with_returning_implementation(std::string_view table_name,
            std::vector<Record>&& rows, const std::vector<std::shared_ptr<FieldBase>>& replace_fields,
            const std::vector<std::shared_ptr<FieldBase>>& returning_fields) override {
            return {};
        }

        // Implementation Methods for Data Manipulation
        void insert_implementation(std::string_view table_name, const std::vector<Record>& rows) override {}

        void insert_implementation(std::string_view table_name, std::vector<Record>&& rows) override {}

        void upsert_implementation(std::string_view table_name, const std::vector<Record>& rows,
            const std::vector<std::shared_ptr<FieldBase>>& replace_fields) override {}

        void upsert_implementation(std::string_view table_name, std::vector<Record>&& rows,
            const std::vector<std::shared_ptr<FieldBase>>& replace_fields) override {}

    private:
        static constexpr char label[] = "[SILENT MOCK DATABASE LOG]:\t";
    };
} // namespace common::database
