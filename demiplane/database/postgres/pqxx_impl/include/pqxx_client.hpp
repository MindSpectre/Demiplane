// pqxx_client.hpp
#pragma once

#include <memory>
#include <mutex>
#include <pqxx/pqxx>
#include <string>
#include <string_view>
#include <vector>

#include "boost/container/flat_map.hpp"
#include "db_connect_params.hpp"
#include "db_exceptions.hpp"
#include "db_interface.hpp"


namespace common::database {
    class PqxxClient final : public interfaces::DbInterface {
    public:
        /// @brief Creating a database with given params using template db
        static void create_database(std::string_view host, uint32_t port, std::string_view db_name,
            std::string_view login, std::string_view password);

        static void create_database(const ConnectParams& pr);

        /// @brief Explicitly close the connection. Destructor does the same, but this function can throw exception
        void drop_connect() override;


        /// @brief Trying to connect to the database, if connection isn't open will throw exception
        /// @throws common::database::exceptions::ConnectionException
        PqxxClient(std::string_view host, uint32_t port, std::string_view db_name, std::string_view login,
            std::string_view password);

        explicit PqxxClient(const ConnectParams& pr);

        ~PqxxClient() override = default;

        // Transaction Methods
        /// @brief Start a transaction. All queries before commiting/roll backing a transaction will use common
        /// transaction
        /// @throws  common::database::exceptions::TransactionException
        void start_transaction() override;

        /// @brief Commit a transaction. If throws an exception, all transactions will be reverted
        /// @throws  common::database::exceptions::TransactionException
        void commit_transaction() override;

        /// @brief Instantly cancel a transaction
        /// @throws  common::database::exceptions::TransactionException
        void rollback_transaction() override;

        /// @brief Create unique index for the table
        /// @param table_name For which table created index.
        /// @param conflict_fields Fields which will be unique for each record
        void make_unique_constraint(std::string_view table_name, FieldCollection conflict_fields) override;

        /// @brief Create full test search index for given fields
        /// @param table_name For which table created index.
        /// @param fields Fts fields
        void setup_search_index(std::string_view table_name, FieldCollection fields) override;

        /// @brief Drop index, but doesn't remove fts fields from this client.
        /// Allows restoring it(reindex) using restore_full_text_search method
        void drop_search_index(std::string_view table_name) const override;

        /// @brief Drop index + remove fields from this client. For using fts further setup_fulltext_search should be
        /// called again
        void remove_search_index(std::string_view table_name) override;

        /// @brief Restore index + reindex. Use previous declared fts fields
        void restore_search_index(std::string_view table_name) const override;

        // Table Management

        /// @param table_name new table name
        /// @param field_list properties of fields of new table
        void create_table(std::string_view table_name, const Record& field_list) override;

        void remove_table(std::string_view table_name) override;

        void truncate_table(std::string_view table_name) override;

        /// @return Existence status of table
        [[nodiscard]] bool check_table(std::string_view table_name) override;

        // Data Retrieval

        /// @brief Select records follow conditions. Copy all data to own it
        /// @return Vector of records. Each element of vector - one row in a table
        [[nodiscard]] Records select(std::string_view table_name, const Conditions& conditions) const override;

        /// @brief Select all table records.
        /// @warning Slow, because of copying all fields
        [[nodiscard]] Records select(std::string_view table_name) const override;

        /// @brief Faster than select, but doesn't transform and allows only one operation view field
        /// @return Vector of view records. Each element of vector - one row in a table
        /// @warning If u needn't only view data, use select.
        [[nodiscard]] ViewRecords view(std::string_view table_name, const Conditions& conditions) const override;

        /// @brief Faster than select, but doesn't transform and allows only one operation view field
        /// @return Vector of view records. Each element of vector - one row in a table
        /// @warning If u needn't only view data, use select.
        [[nodiscard]] ViewRecords view(std::string_view table_name) const override;

        // Remove Data
        ///@brief remove data following conditions
        void remove(std::string_view table_name, const Conditions& conditions) override;

        // Get Record Count
        /// @return Count of records
        [[nodiscard]] uint32_t count(std::string_view table_name, const Conditions& conditions) const override;

        /// @return Count of all records in table
        [[nodiscard]] uint32_t count(std::string_view table_name) const override;

        void set_search_fields(std::string_view table_name, FieldCollection fields) override;

        void set_conflict_fields(std::string_view table_name, FieldCollection fields) override;

    protected:
        // Implementation Methods for Data Manipulation
        void insert_implementation(std::string_view table_name, const Records& rows) override;

        void insert_implementation(std::string_view table_name, Records&& rows) override;

        void upsert_implementation(
            std::string_view table_name, const Records& rows, const FieldCollection& replace_fields) override;

        void upsert_implementation(
            std::string_view table_name, Records&& rows, const FieldCollection& replace_fields) override;

        Records insert_with_returning_implementation(
            std::string_view table_name, const Records& rows, const FieldCollection& returning_fields) override;

        Records insert_with_returning_implementation(
            std::string_view table_name, Records&& rows, const FieldCollection& returning_fields) override;

        Records upsert_with_returning_implementation(std::string_view table_name, const Records& rows,
            const FieldCollection& replace_fields, const FieldCollection& returning_fields) override;

        Records upsert_with_returning_implementation(std::string_view table_name, Records&& rows,
            const FieldCollection& replace_fields, const FieldCollection& returning_fields) override;

    private:
        boost::container::flat_map<uint32_t, std::string> type_oids_; // id, name
        boost::container::flat_map<std::string, FieldCollection> conflict_fields_ = {};
        boost::container::flat_map<std::string, FieldCollection> search_fields_   = {};
        std::shared_ptr<pqxx::connection> conn_;
        mutable std::recursive_mutex conn_mutex_;
        mutable std::unique_ptr<pqxx::work> open_transaction_;
        bool in_transaction_;

        void oid_preprocess();

        [[nodiscard]] std::unique_ptr<FieldBase> process_field(const pqxx::field& field) const;

        // Utility Methods
        [[nodiscard]] static bool is_valid_identifier(std::string_view identifier);

        void execute_query(const std::string& query_string, const pqxx::params& params) const;

        void execute_query(const std::string& query_string) const;

        pqxx::result execute_query_with_result(const std::string& query_string, const pqxx::params& params) const;

        pqxx::result execute_query_with_result(const std::string& query_string) const;

        template <interfaces::RecordContainer Rec>
        std::pair<std::string, pqxx::params> construct_insert_query(
            const std::string_view table_name, Rec&& rows) const {
            if (rows.empty()) {
                throw exceptions::QueryException("No data provided for insert.", errors::db_error_code::INVALID_QUERY);
            }
            const std::string table = escape_identifier(table_name);
            std::ostringstream query_stream;
            query_stream << "INSERT INTO " << table << " (";
            // Get field names from the first record
            const auto& first_record = rows.front();
            std::vector<std::string> field_names;
            for (const auto& it : first_record) {
                field_names.push_back(it->get_name());
                query_stream << escape_identifier(field_names.back()) << ", ";
            }
            std::string query = query_stream.str();
            query.erase(query.size() - 2); // Remove last comma and space
            query += ") VALUES ";


            pqxx::params params;
            uint32_t param_counter = 1;
            for (auto&& record : std::forward<Rec>(rows)) {
                query += "(";
                for (auto&& field : record) {
                    if (field->get_sql_type() == SqlType::UUID && field->to_string() == Uuid::default_value) {
                        query += "DEFAULT, ";
                    } else if (field->get_sql_type() == SqlType::UUID && field->to_string() == Uuid::null_value) {
                        query += "NULL, ";
                    } else {
                        query += "$" + std::to_string(param_counter++) + ", ";
                        // Call the correct version of to_string() depending on the value category
                        params.append(std::forward<decltype(field)>(field)->to_string());
                    }
                }
                query.erase(query.size() - 2); // Remove last comma and space
                query += "), ";
            }
            query.erase(query.size() - 2); // Remove last comma and space
            return {query, params};
        }

        std::string escape_identifier(std::string_view identifier) const;

        std::unique_ptr<pqxx::work> initialize_transaction() const;

        void finish_transaction(std::unique_ptr<pqxx::work>&& current_transaction) const;

        static exceptions::DatabaseException adapt_exception(const std::exception& pqxx_exception);

        void build_conflict_clause_for_force_insert(
            std::string& query, std::string_view table_name, const FieldCollection& replace_fields) const&;

        static void build_returning_clause(std::string& query, const FieldCollection& returning_fields);

        void conditions_to_query(std::string_view table_name, std::ostringstream& query_stream, pqxx::params& params,
            uint32_t& param_index, const Conditions& conditions) const;

        void create_fts_index_query(std::string_view table_name, std::ostringstream& index_query) const;

        void create_trgm_index_query(std::string_view table_name, std::ostringstream& index_query) const;

        static std::string make_fts_index_name(std::string_view table_name);

        static std::string make_trgm_index_name(std::string_view table_name);

        void install_trgm_extension() const;
    };
} // namespace common::database
