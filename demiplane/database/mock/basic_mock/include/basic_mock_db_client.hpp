// pqxx_client.hpp
#pragma once

#include <iostream>
#include <memory>
#include <regex>
#include <string_view>
#include <valarray>
#include <vector>

#include "db_connect_params.hpp"
#include <db_interface.hpp>


namespace demiplane::database {
    class BasicMockDbClient final : public interfaces::DbInterface {
    public:
        /// @brief Creating database with given params using template db
        static void create_database(const ConnectParams& params) {
            std::cout << label << "Creating database " << params.get_host() << ":" << params.get_port()
                      << "\nDatabase name: " << params.get_db_name() << "\nCredentials: " << params.get_login() << " "
                      << "***********" << std::endl;
        }

        /// @brief Explicitly close the connection. Destructor do the same, but this function can throw exception
        void drop_connect() override {
            std::cout << label << "Dropping connect" << std::endl;
        }


        /// @brief Trying to connect to the database, if connection is not open will throw exception
        /// @throws demiplane::database::exceptions::ConnectionException
        explicit BasicMockDbClient(const ConnectParams& params) {
            std::cout << label << "Connecting to the database " << params.get_host() << ":" << params.get_port()
                      << "\nDatabase name: " << params.get_db_name() << "\nCredentials: " << params.get_login() << " "
                      << "***********" << std::endl;
        }

        BasicMockDbClient() {
            std::cout << label << "Connecting to database. Default params(or none) have been set." << std::endl;
        }
        ~BasicMockDbClient() override = default;

        // Transaction Methods
        /// @brief Start a transaction. All queries before commiting/roll backing a transaction will use common
        /// transaction
        /// @throws  demiplane::database::exceptions::TransactionException
        void start_transaction() override {
            std::cout << label << "Starting transaction." << std::endl;
        }

        /// @brief Commit a transaction. If throws an exception, all transaction will be reverted
        /// @throws  demiplane::database::exceptions::TransactionException
        void commit_transaction() override {
            std::cout << label << "Commiting transaction." << std::endl;
        }

        /// @brief Instantly cancel a transaction
        /// @throws  demiplane::database::exceptions::TransactionException
        void rollback_transaction() override {
            std::cout << label << "Rolling back transaction." << std::endl;
        }

        /// @brief Create unique index for the table
        /// @param table_name For which table created index.
        /// @param conflict_fields Fields which will be unique for each record
        void make_unique_constraint(
            std::string_view table_name, std::vector<std::shared_ptr<FieldBase>> conflict_fields) override {
            std::cout << label << "Creating unique constraint." << std::endl;
        }

        /// @brief Create full test search index for given fields
        /// @param table_name For which table created index.
        /// @param fields Fts fields
        void setup_search_index(std::string_view table_name, std::vector<std::shared_ptr<FieldBase>> fields) override {
            std::cout << label << "Setting up search index." << std::endl;
        }

        /// @brief Drop index, but doesn't remove fts fields from this client. Allows to restore it(reindex) using
        /// restore_full_text_search method
        void drop_search_index(std::string_view table_name) const override {
            std::cout << label << "Dropping search index(temporary)." << std::endl;
        }

        /// @brief Drop index + remove fields from this client. For using fts further setup_fulltext_search should be
        /// called again
        void remove_search_index(std::string_view table_name) override {
            std::cout << label << "Removing search index(forever)." << std::endl;
        }

        /// @brief Restore index + reindex. Use previous declared fts fields
        void restore_search_index(std::string_view table_name) const override {
            std::cout << label << "Restoring search index." << std::endl;
        }

        // Table Management

        /// @param table_name new table name
        /// @param field_list properties of fields of new table
        void create_table(std::string_view table_name, const Record& field_list) override {
            std::cout << label << "Creating table." << std::endl;
        }

        void remove_table(std::string_view table_name) override {
            std::cout << label << "Removing table." << std::endl;
        }

        void truncate_table(std::string_view table_name) override {
            std::cout << label << "Truncating table." << std::endl;
        }

        /// @return Existence status of table
        [[nodiscard]] bool check_table(std::string_view table_name) override {
            std::cout << label << "Checking table existence." << std::endl;
            return true;
        }

        // Data Retrieval

        /// @brief Select records follow conditions. Copy all data to own it
        /// @return Vector of records. Each element of vector - one row in a table
        [[nodiscard]] std::vector<Record> select(
            std::string_view table_name, const Conditions& conditions) const override {
            std::cout << label << "Selecting." << std::endl;
            return {};
        }

        /// @brief Select all table records.
        /// @warning Slow, because of copying all fields
        [[nodiscard]] std::vector<Record> select(std::string_view table_name) const override {
            std::cout << label << "Selecting." << std::endl;
            return {};
        }


        // Remove Data
        ///@brief remove data following conditions
        void remove(std::string_view table_name, const Conditions& conditions) override {
            std::cout << label << "Removing." << std::endl;
        }

        // Get Record Count
        /// @return Count of records
        [[nodiscard]] uint32_t count(std::string_view table_name, const Conditions& conditions) const override {
            std::cout << label << "Counting." << std::endl;
            return 0;
        }

        /// @return Count of all records in table
        [[nodiscard]] uint32_t count(std::string_view table_name) const override {
            std::cout << label << "Counting." << std::endl;
            return 0;
        }

        void set_search_fields(std::string_view table_name, std::vector<std::shared_ptr<FieldBase>> fields) override {
            std::cout << label << "Setting search fields." << std::endl;
        }

        void set_conflict_fields(std::string_view table_name, std::vector<std::shared_ptr<FieldBase>> fields) override {
            std::cout << label << "Setting conflict fields." << std::endl;
        }

    protected:
        std::vector<Record> insert_with_returning_implementation(std::string_view table_name,
            const std::vector<Record>& rows, const std::vector<std::shared_ptr<FieldBase>>& returning_fields) override {
            std::cout << label << "insert_with_returning_implementation" << std::endl;
            return {};
        }

        std::vector<Record> insert_with_returning_implementation(std::string_view table_name,
            std::vector<Record>&& rows, const std::vector<std::shared_ptr<FieldBase>>& returning_fields) override {
            std::cout << label << "insert_with_returning_implementation" << std::endl;
            return {};
        }

        std::vector<Record> upsert_with_returning_implementation(std::string_view table_name,
            const std::vector<Record>& rows, const std::vector<std::shared_ptr<FieldBase>>& replace_fields,
            const std::vector<std::shared_ptr<FieldBase>>& returning_fields) override {
            std::cout << label << "upsert_with_returning_implementation" << std::endl;
            return {};
        }

        std::vector<Record> upsert_with_returning_implementation(std::string_view table_name,
            std::vector<Record>&& rows, const std::vector<std::shared_ptr<FieldBase>>& replace_fields,
            const std::vector<std::shared_ptr<FieldBase>>& returning_fields) override {
            std::cout << label << "upsert_with_returning_implementation" << std::endl;
            return {};
        }

        // Implementation Methods for Data Manipulation
        void insert_implementation(std::string_view table_name, const std::vector<Record>& rows) override {
            std::cout << label << "insert_implementation " << std::endl;
        }

        void insert_implementation(std::string_view table_name, std::vector<Record>&& rows) override {
            std::cout << label << "insert_implementation " << std::endl;
        }

        void upsert_implementation(std::string_view table_name, const std::vector<Record>& rows,
            const std::vector<std::shared_ptr<FieldBase>>& replace_fields) override {
            std::cout << label << "upsert_implementation " << std::endl;
        }

        void upsert_implementation(std::string_view table_name, std::vector<Record>&& rows,
            const std::vector<std::shared_ptr<FieldBase>>& replace_fields) override {
            std::cout << label << "upsert_implementation " << std::endl;
        }

    private:
        static constexpr char label[] = "[MOCK DATABASE LOG]:\t";
    };
} // namespace demiplane::database
