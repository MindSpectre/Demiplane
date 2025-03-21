// pqxx_client.cpp

#include "pqxx_client.hpp"

#include <chrono>
#include <iostream>
#include <regex>
#include <sstream>

#include "db_conditions.hpp"
#include "db_field.hpp"
#include "pqxx_view_record.hpp"
#include "stopwatch.hpp"

namespace demiplane::database {
    using namespace exceptions;
    using db_err = errors::db_error_code;
    // Constructors
    PqxxClient::PqxxClient(const std::string_view host, const uint32_t port, const std::string_view db_name,
        const std::string_view login, const std::string_view password)
        : in_transaction_(false) {
        try {
            std::ostringstream conn_str;
            conn_str << "host=" << host << " port=" << port << " user=" << login << " password=" << password
                     << " dbname=" << db_name;

            this->conn_ = std::make_shared<pqxx::connection>(conn_str.str());
            if (!this->conn_->is_open()) {
                throw ConnectionException("Failed to open database connection.", db_err::CONNECTION_FAILED);
            }
            oid_preprocess();
        } catch (const pqxx::too_many_connections& e) {
            throw ConnectionException(e.what(), db_err::CONNECTION_POOL_EXHAUSTED);
        } catch (const pqxx::sql_error& e) {
            throw ConnectionException(e.what(), db_err::INVALID_QUERY);
        } catch (const std::exception& e) {
            throw ConnectionException(e.what(), db_err::PERMISSION_DENIED);
        }
    }


    PqxxClient::PqxxClient(const ConnectParams& pr) : in_transaction_(false) {
        try {
            this->conn_ = std::make_shared<pqxx::connection>(pr.make_connect_string());
            if (!this->conn_->is_open()) {
                throw ConnectionException("Failed to open database connection.", db_err::CONNECTION_FAILED);
            }
            oid_preprocess();
        } catch (const pqxx::too_many_connections& e) {
            throw ConnectionException(e.what(), db_err::CONNECTION_POOL_EXHAUSTED);
        } catch (const pqxx::sql_error& e) {
            throw ConnectionException(e.what(), db_err::INVALID_QUERY);
        } catch (const std::exception& e) {
            throw ConnectionException(e.what(), db_err::PERMISSION_DENIED);
        }
    }

    // Utility method to check if an identifier is valid
    bool PqxxClient::is_valid_identifier(const std::string_view identifier) {
        static const std::regex valid_identifier_regex("^[a-zA-Z_][a-zA-Z0-9_]*$");
        return std::regex_match(identifier.begin(), identifier.end(), valid_identifier_regex);
    }

    // Utility method to escape identifiers
    std::string PqxxClient::escape_identifier(const std::string_view identifier) const {
        if (!is_valid_identifier(identifier)) {
            throw InvalidIdentifierException(std::string(identifier), db_err::INVALID_QUERY);
        }
        return this->conn_->quote_name(std::string(identifier));
    }

    // Static create postgresql Database
    void PqxxClient::create_database(const std::string_view host, const uint32_t port, const std::string_view db_name,
        const std::string_view login, const std::string_view password)
    // Build the connection string to connect to the 'template1' database
    {
        std::ostringstream conn_str;
        conn_str << "host=" << host << " port=" << port << " user=" << login << " password=" << password
                 << " dbname=template1";
        // Connect to 'template1' to create a new database

        try {
            // Establish connection to the 'template1' database

            if (pqxx::connection trivial(conn_str.str()); trivial.is_open()) {
                // Start a non-transactional query to create the database
                pqxx::nontransaction db_creating(trivial);

                // Prepare the SQL query
                std::ostringstream query;
                query << "CREATE DATABASE " << db_name << " WITH OWNER = " << login << " " << "ENCODING = 'UTF8' "
                      << "TEMPLATE template0;"; // Use template0 for a clean database
                db_creating.exec(query.str());
                db_creating.commit();
                std::cout << "Database " << db_name << " created successfully!" << std::endl;
            } else {
                std::cerr << "Connection to the database failed." << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "Error creating database: " << e.what() << std::endl;
            throw; // Rethrow the exception if needed
        }
    }


    void PqxxClient::create_database(const ConnectParams& pr) {
        create_database(pr.get_host(), pr.get_port(), pr.get_db_name(), pr.get_login(), pr.get_password());
    }

    //
    void PqxxClient::drop_connect() {
        std::lock_guard lock(this->conn_mutex_);
        this->conn_->close();
    }


    std::unique_ptr<pqxx::work> PqxxClient::initialize_transaction() const {
        if (this->in_transaction_) {
            return std::move(this->open_transaction_);
        }

        try {
            auto one_query_transaction = std::make_unique<pqxx::work>(*this->conn_);
            return one_query_transaction;
        } catch (const pqxx::broken_connection& e) {
            throw ConnectionException(e.what(), db_err::CONNECTION_FAILED);
        } catch (std::exception& e) {
            throw TransactionException(
                std::string(e.what()).append(". Undefined exception."), db_err::TRANSACTION_START_FAILED);
        }
    }

    void PqxxClient::finish_transaction(std::unique_ptr<pqxx::work>&& current_transaction) const {
        if (this->in_transaction_) {
            this->open_transaction_ = std::move(current_transaction);
            return;
        }
        try {
            current_transaction->commit();
            current_transaction.reset();
        } catch (const pqxx::broken_connection& e) {
            throw ConnectionException(e.what(), db_err::CONNECTION_FAILED);
        } catch (std::exception& e) {
            throw TransactionException(
                std::string(e.what()).append(". Undefined exception."), db_err::TRANSACTION_COMMIT_FAILED);
        }
    }

    DatabaseException PqxxClient::adapt_exception(const std::exception& pqxx_exception) {
        try {
            throw; // Re-throws the exception
        } catch (const pqxx::deadlock_detected& e) {
            throw DatabaseException(e.what(), db_err::DEADLOCK_DETECTED);
        } catch (const pqxx::transaction_rollback& e) {
            throw DatabaseException(e.what(), db_err::SYSTEM_ROLLBACK);
        } catch (const pqxx::syntax_error& e) {
            throw DatabaseException(e.what(), db_err::INVALID_QUERY);
        } catch (const pqxx::data_exception& e) {
            throw DatabaseException(e.what(), db_err::INVALID_DATA);
        } catch (const pqxx::sql_error& e) {
            throw DatabaseException(e.what(), db_err::QUERY_EXECUTION_FAILED);
        } catch (...) {
            throw; // Rethrow unknown exception
        }
    }

    void PqxxClient::build_conflict_clause_for_force_insert(
        std::string& query, const std::string_view table_name, const FieldCollection& replace_fields) const& {
        // Build ON CONFLICT clause
        std::ostringstream conflict_stream;
        FieldCollection conflict_fields;
        {
            std::lock_guard lock(this->conn_mutex_);
            try {
                conflict_fields = this->conflict_fields_.at(table_name.data());
            } catch (boost::container::out_of_range&) {
                throw QueryException("For this table upsert clause is not set up. Or invalid table name credentials",
                    db_err::INVALID_DATA);
            }
        }
        conflict_stream << " ON CONFLICT (";
        for (const auto& field : conflict_fields) {
            conflict_stream << escape_identifier(field->get_name()) << ", ";
        }
        std::string conflict_clause = conflict_stream.str();
        conflict_clause.erase(conflict_clause.size() - 2); // Remove last comma and space
        conflict_clause += ") DO UPDATE SET ";

        // Build SET clause
        for (const auto& field : replace_fields) {
            std::string name = escape_identifier(field->get_name());
            conflict_clause.append(name).append(" = EXCLUDED.").append(name).append(", ");
        }
        conflict_clause.erase(conflict_clause.size() - 2); // Remove last comma and space

        query += conflict_clause;
    }

    void PqxxClient::build_returning_clause(std::string& query, const FieldCollection& returning_fields) {
        std::ostringstream returning_stream;
        returning_stream << " RETURNING ";
        for (const auto& field : returning_fields) {
            returning_stream << field->get_name() << ", ";
        }
        query += returning_stream.str();
        query.resize(query.size() - 2);
    }

    void PqxxClient::conditions_to_query(const std::string_view table_name, std::ostringstream& query_stream,
        pqxx::params& params, uint32_t& param_index, const Conditions& conditions) const {
        // lambdas
        auto process_fields_clause = [&](const std::list<FieldCondition>& fields_clause) {
            std::optional<std::string> query;
            if (fields_clause.empty()) {
                return query;
            }
            std::ostringstream local_stream;
            for (const auto& condition : fields_clause) {
                std::string field_name = escape_identifier(condition.name());
                local_stream << field_name << " " << condition.op() << " $" << param_index++ << " AND ";
                params.append(condition.value());
            }
            query = local_stream.str();
            return query;
        };

        auto process_patterns_clause = [&](const std::list<PatternCondition>& patterns_clause) {
            std::optional<std::string> query;
            if (patterns_clause.empty()) {
                return query;
            }
            std::ostringstream local_stream;
            std::ostringstream fts_fields_stream;
            FieldCollection fts_fields;
            {
                std::lock_guard lock(this->conn_mutex_);
                try {
                    fts_fields = this->search_fields_.at(table_name.data());
                } catch (std::out_of_range&) {
                    throw QueryException(
                        "For this table fts index is not set up or disabled. Or invalid table name credentials",
                        db_err::INVALID_DATA);
                }
            }
            for (const auto& field : fts_fields) {
                fts_fields_stream << "coalesce(" << field->get_name() << "::text, '') || ' ' || ";
            }
            std::string fields_concatenated = fts_fields_stream.str();
            fields_concatenated.erase(fields_concatenated.size() - 11); // Remove last " || ' ' || "
            std::ostringstream tsquery_stream;
            for (const auto& pattern : patterns_clause) {
                tsquery_stream << pattern.get_pattern() << " & ";
            }
            std::string tsquery = tsquery_stream.str();
            tsquery.erase(tsquery.size() - 3); // Remove last " & "
            local_stream << "to_tsvector('simple', " << fields_concatenated << ") @@ to_tsquery('simple', $"
                         << param_index++ << ") AND ";
            params.append(tsquery);
            query = local_stream.str();
            return query;
        };
        auto process_order_by_clause = [&](const std::list<OrderCondition>& order_by_clause) {
            std::optional<std::string> query;
            std::ostringstream local_stream;
            if (order_by_clause.empty()) {
                return query;
            }
            for (const auto& condition : order_by_clause) {
                local_stream << escape_identifier(condition.get_column()->get_name()) << " ";
                switch (condition.get_order()) {
                case order_type::ascending:
                    {
                        local_stream << "ASC, ";
                        break;
                    }
                case order_type::descending:
                    {
                        local_stream << "DESC, ";
                        break;
                    }
                }
            }
            query = local_stream.str();
            return query;
        };

        auto process_paging_clause = [&](const PageCondition& paging) {
            std::ostringstream local_stream;
            local_stream << " LIMIT " << paging.get_limit() << " OFFSET " << paging.get_offset();
            return local_stream.str();
        };

        auto process_similarity_clause = [&](const std::list<SimilarityCondition>& similarity_conditions)

        {
            std::optional<std::string> res;
            // If no patterns, return empty clauses
            if (similarity_conditions.empty()) {
                return res;
            }
            std::ostringstream order_by_clause_stream;
            std::ostringstream fields_stream;
            FieldCollection search_fields;
            {
                std::lock_guard lock(this->conn_mutex_);
                try {
                    // Retrieve searchable fields for the table
                    search_fields = this->search_fields_.at(table_name.data());
                } catch (std::out_of_range&) {
                    throw QueryException(
                        "For this table, search fields are not set up or invalid table name credentials.",
                        db_err::INVALID_DATA);
                }
            }

            // Concatenate all fields into a single expression
            for (const auto& field : search_fields) {
                fields_stream << "coalesce(" << field->get_name() << "::text, '') || ' ' || ";
            }
            std::string fields_concatenated = fields_stream.str();
            if (!fields_concatenated.empty()) {
                fields_concatenated.erase(fields_concatenated.size() - 11); // Remove last " || ' ' || "
            } else {
                throw QueryException("No valid searchable fields defined for the table.", db_err::INVALID_DATA);
            }

            // Build the trigram query
            std::ostringstream pattern_stream;
            for (const auto& pattern : similarity_conditions) {
                pattern_stream << pattern.get_pattern() << " | ";
            }
            std::string combined_pattern = pattern_stream.str();
            if (!combined_pattern.empty()) {
                combined_pattern.erase(combined_pattern.size() - 3); // Remove last " | "
            } else {
                throw QueryException("Pattern conditions are empty or invalid.", db_err::INVALID_DATA);
            }

            // WHERE clause for trigram search
            params.append(combined_pattern);

            // ORDER BY clause for ranking based on similarity
            order_by_clause_stream << "(" << fields_concatenated << ") <-> $" << param_index++ << ", ";
            res = order_by_clause_stream.str();
            // Return WHERE and ORDER BY clauses
            return res;
        };
        auto order_by_similarity_clause = process_similarity_clause(conditions.similarity_conditions());
        if (const std::optional<std::string> patterns_clause = process_patterns_clause(conditions.pattern_conditions()),
            fields_clause                                    = process_fields_clause(conditions.fields_conditions());
            fields_clause.has_value() || patterns_clause.has_value()) {
            std::ostringstream where_stream;
            where_stream << " WHERE ";
            if (fields_clause.has_value()) {
                where_stream << fields_clause.value();
            }
            if (patterns_clause.has_value()) {
                where_stream << patterns_clause.value();
            }
            std::string tmp = where_stream.str();
            tmp.erase(tmp.size() - 5);
            query_stream << tmp;
        }
        if (std::optional<std::string> order_by_clause = process_order_by_clause(conditions.order_by_conditions());
            order_by_clause.has_value() || order_by_similarity_clause.has_value()) {
            std::ostringstream order_by;
            order_by << " ORDER BY ";
            if (order_by_clause.has_value()) {
                order_by << order_by_clause.value();
            }
            if (order_by_similarity_clause.has_value()) {
                order_by << order_by_similarity_clause.value();
            }
            std::string tmp = order_by.str();
            tmp.erase(tmp.size() - 2);
            query_stream << tmp;
        }
        if (conditions.page_condition().has_value()) {
            query_stream << process_paging_clause(conditions.page_condition().value());
        }
    }

    // Utility method to execute a query
    void PqxxClient::execute_query(const std::string& query_string, const pqxx::params& params) const {
        std::lock_guard lock(this->conn_mutex_);
        try {
            std::unique_ptr<pqxx::work> txn = initialize_transaction();
            txn->exec_params(query_string, params);
            finish_transaction(std::move(txn));
        } catch (const std::exception& e) {
            throw adapt_exception(e);
        }
    }


    void PqxxClient::execute_query(const std::string& query_string) const {
        std::lock_guard lock(this->conn_mutex_);
        try {
            std::unique_ptr<pqxx::work> txn = initialize_transaction();
            txn->exec(query_string);
            finish_transaction(std::move(txn));
        } catch (const std::exception& e) {
            throw adapt_exception(e);
        }
    }

    pqxx::result PqxxClient::execute_query_with_result(
        const std::string& query_string, const pqxx::params& params) const {
        std::lock_guard lock(this->conn_mutex_);
        try {
            std::unique_ptr<pqxx::work> txn = initialize_transaction();
            const pqxx::result response     = txn->exec_params(query_string, params);
            finish_transaction(std::move(txn));
            return response;
        } catch (const std::exception& e) {
            throw adapt_exception(e);
        }
    }

    pqxx::result PqxxClient::execute_query_with_result(const std::string& query_string) const {
        std::lock_guard lock(this->conn_mutex_);
        try {
            std::unique_ptr<pqxx::work> txn = initialize_transaction();
            const pqxx::result response     = txn->exec(query_string);
            finish_transaction(std::move(txn));
            return response;
        } catch (const std::exception& e) {
            throw adapt_exception(e);
        }
    }

    void PqxxClient::oid_preprocess() {
        try {
            std::lock_guard lock(this->conn_mutex_);
            pqxx::nontransaction txn(*this->conn_);
            const pqxx::result r =
                txn.exec("SELECT typname, oid FROM pg_type WHERE typname IN ('bool', 'int2', 'int4', 'int8', "
                         "'float4', 'float8', 'text', 'varchar', 'bpchar', 'timestamp', 'timestamptz', 'uuid', 'json', "
                         "'jsonb')");

            for (const auto& row : r) {
                this->type_oids_.insert({row["oid"].as<uint32_t>(), row["typname"].c_str()});
            }
        } catch (const std::exception& e) {
            throw adapt_exception(e);
        }
    }

    UniqueFieldPtr PqxxClient::process_field(const pqxx::field& field) const {
        UniqueFieldPtr field_ptr;
        using factory = unique_field_factory;
        // Get the field's PostgresSQL type OID
        boost::container::flat_map<uint32_t, std::string>::const_iterator type_oid;
        {
            std::lock_guard lock(this->conn_mutex_);
            type_oid = this->type_oids_.find(field.type());
            if (type_oid == this->type_oids_.end()) {
                throw std::invalid_argument("field type not found");
            }
        }

        // Determine the C++ type based on the PostgresSQL type OID and create the appropriate Field object
        if (type_oid->second == "int4") // 32-bit integer
        {
            field_ptr = factory::int_field(field.name(), field.as<int32_t>());
            return field_ptr;
        }
        // Handle string types (text, varchar, char)
        if (type_oid->second == "text") // Text type
        {
            field_ptr = factory::text_field(field.name(), field.as<std::string>());
            return field_ptr;
        }

        if (type_oid->second == "int8") // 64-bit integer (Bigint)
        {
            field_ptr = factory::ll_int_field(field.name(), field.as<int64_t>());
            return field_ptr;
        }

        // Handle float types
        if (type_oid->second == "jsonb" || type_oid->second == "json") // Double precision (64-bit float)
        {
            Json::Value json;
            Json::CharReaderBuilder reader_builder;
            std::string errs;
            if (std::istringstream json_stream(field.c_str());
                !parseFromStream(reader_builder, json_stream, &json, &errs)) {
                throw std::runtime_error("Failed to parse JSON: " + errs);
            }
            field_ptr = factory::json_field(field.name(), json);
            return field_ptr;
        }
        if (type_oid->second == "float8") // Double precision (64-bit float)
        {
            field_ptr = factory::double_field(field.name(), field.as<double>());
            return field_ptr;
        }

        // Handle boolean type
        if (type_oid->second == "bool") // Boolean type
        {
            field_ptr = factory::bool_field(field.name(), field.as<bool>());
            return field_ptr;
        }
        // Handle UUID type
        if (type_oid->second == "uuid") // UUID type
        {
            // WARNING All fields are identified as not primary, suppose be checked further
            if (field.is_null() == true) {
                field_ptr = factory::uuid_field(field.name(), Uuid().set_null());
                return field_ptr;
            }
            field_ptr = factory::uuid_field(field.name(), Uuid(field.as<std::string>(), false));
            return field_ptr;
        }
        // Handle timestamp types (with or without a timezone)
        if (type_oid->second == "timestamptz" || type_oid->second == "timestamp") // Timestamp type
        {
            const auto value = field.as<std::string>();
            // Parse the timestamp string into std::chrono::system_clock::time_point
            std::istringstream iss(value);
            std::tm tm = {};
            iss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S"); // Parse the string to fill the tm structure
            auto tp   = std::chrono::system_clock::from_time_t(std::mktime(&tm)); // Convert to time_point
            field_ptr = factory::time_field(field.name(), tp);
            return field_ptr;
        }

        // Return the field pointer (nullptr if no matching type was found)
        return field_ptr;
    }

    void PqxxClient::make_unique_constraint(const std::string_view table_name, FieldCollection conflict_fields) {
        FieldCollection conflict_fields_it;
        {
            std::lock_guard lock(this->conn_mutex_);
            std::string tb             = table_name.data();
            this->conflict_fields_[tb] = std::move(conflict_fields);
            conflict_fields_it         = this->conflict_fields_.at(tb);
        }
        const std::string table = escape_identifier(table_name);
        std::ostringstream query_stream;
        std::ostringstream sub_query;
        query_stream << "ALTER TABLE " << table << " ADD CONSTRAINT ";
        std::ostringstream name_constraint;
        sub_query << " UNIQUE (";
        for (const auto& column : conflict_fields_it) {
            sub_query << escape_identifier(column->get_name()) << ",";
            name_constraint << column->get_name() << "_";
        }
        name_constraint << table_name;
        std::string query = query_stream.str();
        query.append(escape_identifier(name_constraint.str())).append(sub_query.str());
        query.pop_back();
        query += ");";

        execute_query(query);
    }

    // Transaction Methods
    void PqxxClient::start_transaction() {
        if (this->in_transaction_) {
            throw TransactionException("Transaction already started.", db_err::TRANSACTION_START_FAILED);
        }
        try {
            std::lock_guard lock(this->conn_mutex_);
            this->open_transaction_ = std::make_unique<pqxx::work>(*this->conn_);
            this->in_transaction_   = true;
        } catch (const pqxx::sql_error& e) {
            throw TransactionException(e.what(), db_err::QUERY_EXECUTION_FAILED);
        }
    }

    void PqxxClient::commit_transaction() {
        if (!this->in_transaction_) {
            throw TransactionException("No active transaction to commit.", db_err::TRANSACTION_COMMIT_FAILED);
        }
        try {
            std::lock_guard lock(this->conn_mutex_);
            this->open_transaction_->commit();
            this->open_transaction_.reset();
            this->in_transaction_ = false;
        } catch (const pqxx::sql_error& e) {
            throw TransactionException(e.what(), db_err::QUERY_EXECUTION_FAILED);
        }
    }

    void PqxxClient::rollback_transaction() {
        if (!this->in_transaction_) {
            throw TransactionException("No active transaction to rollback.", db_err::TRANSACTION_ROLLBACK_FAILED);
        }
        try {
            std::lock_guard lock(this->conn_mutex_);
            this->open_transaction_->abort();
            this->open_transaction_.reset();
            this->in_transaction_ = false;
        } catch (const pqxx::sql_error& e) {
            throw TransactionException(e.what(), db_err::QUERY_EXECUTION_FAILED);
        }
    }


    // Table Management
    void PqxxClient::create_table(const std::string_view table_name, const Record& field_list) {
        const std::string table = escape_identifier(table_name);
        std::ostringstream query_stream;
        query_stream << "CREATE TABLE " << table << " (";

        for (const auto& field : field_list) {
            std::string name = escape_identifier(field->get_name());
            std::string type = field->get_sql_type_initialization();
            query_stream << name << " " << type << ", ";
        }

        std::string query = query_stream.str();
        query.erase(query.size() - 2); // Remove last comma and space
        query += ");";

        execute_query(query);
    }

    void PqxxClient::remove_table(const std::string_view table_name) {
        const std::string table = escape_identifier(table_name);
        std::ostringstream query_stream;
        query_stream << "DROP TABLE IF EXISTS " << table;
        execute_query(query_stream.str());
    }

    bool PqxxClient::check_table(const std::string_view table_name) {
        const std::string table = this->conn_->esc(std::string(table_name));
        const std::string query = "SELECT EXISTS (SELECT 1 FROM information_schema.tables WHERE table_name = "
                                + this->conn_->quote(table) + ");";
        const pqxx::result res = execute_query_with_result(query);
        return res[0][0].as<bool>();
    }

    void PqxxClient::truncate_table(const std::string_view table_name) {
        const std::string table = escape_identifier(table_name);
        std::ostringstream query_stream;
        query_stream << "TRUNCATE TABLE " << table;
        execute_query(query_stream.str());
    }

    // Data Manipulation Implementation
    void PqxxClient::insert_implementation(const std::string_view table_name, const Records& rows) {
        auto [query, params] = construct_insert_query(table_name, rows);
        execute_query(query, params);
    }

    void PqxxClient::insert_implementation(const std::string_view table_name, Records&& rows) {
        auto [query, params] = construct_insert_query(table_name, std::move(rows));
        execute_query(query, params);
    }

    // Upsert Data Implementation
    void PqxxClient::upsert_implementation(
        const std::string_view table_name, const Records& rows, const FieldCollection& replace_fields) {
        auto [query, params] = construct_insert_query(table_name, rows);
        build_conflict_clause_for_force_insert(query, table_name, replace_fields);
        execute_query(query, params);
    }

    void PqxxClient::upsert_implementation(
        const std::string_view table_name, Records&& rows, const FieldCollection& replace_fields) {
        auto [query, params] = construct_insert_query(table_name, rows);
        build_conflict_clause_for_force_insert(query, table_name, replace_fields);
        execute_query(query, params);
    }

    Records PqxxClient::insert_with_returning_implementation(
        const std::string_view table_name, const Records& rows, const FieldCollection& returning_fields) {
        auto [query, params] = construct_insert_query(table_name, rows);
        build_returning_clause(query, returning_fields);
        Records results;
        const pqxx::result res = execute_query_with_result(query, params);
        results.reserve(res.size());
        for (const auto& row : res) {
            Record record;
            record.reserve(row.size());
            for (const auto& field : row) {
                record.push_back(process_field(field));
            }
            results.push_back(std::move(record));
        }
        return results;
    }

    Records PqxxClient::insert_with_returning_implementation(
        const std::string_view table_name, Records&& rows, const FieldCollection& returning_fields) {
        auto [query, params] = construct_insert_query(table_name, std::move(rows));
        build_returning_clause(query, returning_fields);
        Records results;
        const pqxx::result res = execute_query_with_result(query, params);
        results.reserve(res.size());
        for (const auto& row : res) {
            Record record;
            record.reserve(row.size());
            for (const auto& field : row) {
                record.push_back(process_field(field));
            }
            results.push_back(std::move(record));
        }
        return results;
    }

    Records PqxxClient::upsert_with_returning_implementation(const std::string_view table_name, const Records& rows,
        const FieldCollection& replace_fields, const FieldCollection& returning_fields) {
        auto [query, params] = construct_insert_query(table_name, rows);
        build_conflict_clause_for_force_insert(query, table_name, replace_fields);
        build_returning_clause(query, returning_fields);
        Records results;
        const pqxx::result res = execute_query_with_result(query, params);
        results.reserve(res.size());
        for (const auto& row : res) {
            Record record;
            record.reserve(row.size());
            for (const auto& field : row) {
                record.push_back(process_field(field));
            }
            results.push_back(std::move(record));
        }
        return results;
    }

    Records PqxxClient::upsert_with_returning_implementation(const std::string_view table_name, Records&& rows,
        const FieldCollection& replace_fields, const FieldCollection& returning_fields) {
        auto [query, params] = construct_insert_query(table_name, rows);
        build_conflict_clause_for_force_insert(query, table_name, replace_fields);
        build_returning_clause(query, returning_fields);
        Records results;
        const pqxx::result res = execute_query_with_result(query, params);
        results.reserve(res.size());
        for (const auto& row : res) {
            Record record;
            record.reserve(row.size());
            for (const auto& field : row) {
                record.push_back(process_field(field));
            }
            results.push_back(std::move(record));
        }
        return results;
    }

    void PqxxClient::create_fts_index_query(const std::string_view table_name, std::ostringstream& index_query) const {
        const std::string table = escape_identifier(table_name);

        // Build the concatenated fields expression
        std::ostringstream fields_stream;
        FieldCollection fts_fields;
        {
            std::lock_guard lock(this->conn_mutex_);
            try {
                fts_fields = this->search_fields_.at(table_name.data());
            } catch (std::out_of_range&) {
                throw QueryException(
                    "For this table fts index is not set up or disabled. Or invalid table name credentials",
                    db_err::INVALID_DATA);
            }
        }
        for (const auto& field : fts_fields) {
            fields_stream << "coalesce(" << escape_identifier(field->get_name()) << "::text, '') || ' ' || ";
        }
        std::string fields_concatenated = fields_stream.str();
        fields_concatenated.erase(fields_concatenated.size() - 11); // Remove last " || ' ' || "

        index_query << "CREATE INDEX IF NOT EXISTS " << make_fts_index_name(table_name) << " ON " << table
                    << " USING gin (to_tsvector('simple', " << fields_concatenated << "));";
    }

    void PqxxClient::create_trgm_index_query(const std::string_view table_name, std::ostringstream& index_query) const {
        const std::string table = escape_identifier(table_name);

        // Build the concatenated fields expression
        std::ostringstream fields_stream;
        FieldCollection trgm_fields;
        {
            std::lock_guard lock(this->conn_mutex_);
            try {
                trgm_fields = this->search_fields_.at(table_name.data());
            } catch (std::out_of_range&) {
                throw QueryException(
                    "For this table trigram index is not set up or disabled. Or invalid table name credentials",
                    db_err::INVALID_DATA);
            }
        }
        for (const auto& field : trgm_fields) {
            fields_stream << "coalesce(" << escape_identifier(field->get_name()) << "::text, '') || ' ' || ";
        }
        std::string fields_concatenated = fields_stream.str();
        fields_concatenated.erase(fields_concatenated.size() - 11); // Remove last " || ' ' || "

        index_query << "CREATE INDEX IF NOT EXISTS " << make_trgm_index_name(table_name) << " ON " << table
                    << " USING gin ((" << fields_concatenated << ") gin_trgm_ops);";
    }


    void PqxxClient::setup_search_index(const std::string_view table_name, FieldCollection fields) {
        if (fields.empty()) {
            throw QueryException("Invalid number of fts fields. Expected at least one element", db_err::INVALID_QUERY);
        }
        {
            std::lock_guard lock(this->conn_mutex_);
            this->search_fields_[table_name.data()] = std::move(fields);
        }
        try {
            std::ostringstream fts_index_query;
            create_fts_index_query(table_name, fts_index_query);
            execute_query(fts_index_query.str());
            std::ostringstream trgm_index_query;
            create_trgm_index_query(table_name, trgm_index_query);
            try {
                execute_query(trgm_index_query.str());
            } catch (std::exception& e) {
                std::cerr << e.what() << std::endl;
                std::cerr << "Error during setting up trgm index. Resolving....\n" << std::endl;
                std::cerr << "Installing trgm_extension..." << std::endl;
                install_trgm_extension();
                execute_query(trgm_index_query.str());
                std::cerr << "Successfully finished." << std::endl;
            }
        } catch (const std::exception& e) {
            throw adapt_exception(e);
        }
    }

    void PqxxClient::drop_search_index(const std::string_view table_name) const {
        try {
            std::ostringstream fts_index_query;
            fts_index_query << "DROP INDEX IF EXISTS " << make_fts_index_name(table_name);
            execute_query(fts_index_query.str());
            std::ostringstream trgm_index_query;
            trgm_index_query << "DROP INDEX IF EXISTS " << make_trgm_index_name(table_name);
            execute_query(trgm_index_query.str());
        } catch (const std::exception& e) {
            throw adapt_exception(e);
        }
    }

    void PqxxClient::remove_search_index(const std::string_view table_name) {
        {
            std::lock_guard lock(this->conn_mutex_);
            this->search_fields_[std::string(table_name)].clear();
        }
        drop_search_index(table_name);
    }

    void PqxxClient::restore_search_index(const std::string_view table_name) const {
        try {
            std::ostringstream index_query;
            create_fts_index_query(table_name, index_query);
            execute_query(index_query.str());
            std::ostringstream trgm_index_query;
            create_trgm_index_query(table_name, trgm_index_query);
            execute_query(trgm_index_query.str());
        } catch (const QueryException& e) {
            if (e.get_error() == errors::db_error_code::INVALID_DATA) {
                throw QueryException(
                    "Tried to restore fts index, but fts fields were not set up. Use setup FTS function",
                    errors::db_error_code::INVALID_DATA);
            }
            throw adapt_exception(e);
        } catch (const std::exception& e) {
            throw adapt_exception(e);
        }
    }

    // Data Retrieval
    Records PqxxClient::select(const std::string_view table_name, const Conditions& conditions) const {
        if (conditions.empty()) {
            throw QueryException(
                "Invalid number of conditions. For selecting all data call another function", db_err::INVALID_QUERY);
        }
        Records results;
        const std::string table = escape_identifier(table_name);
        std::ostringstream query_stream;

        pqxx::params params;
        query_stream << "SELECT * FROM " << table;

        uint32_t param_index = 1;
        conditions_to_query(table_name, query_stream, params, param_index, conditions);
        const pqxx::result res = execute_query_with_result(query_stream.str(), params);
        results.reserve(res.size());
        for (const auto& row : res) {
            Record record;
            record.reserve(row.size());
            for (const auto& field : row) {
                record.push_back(process_field(field));
            }
            results.push_back(std::move(record));
        }
        return results;
    }

    ViewRecords PqxxClient::view(const std::string_view table_name, const Conditions& conditions) const {
        if (conditions.empty()) {
            throw QueryException(
                "Invalid number of conditions. For selecting all data call another function", db_err::INVALID_QUERY);
        }
        ViewRecords results;
        const std::string table = escape_identifier(table_name);
        std::ostringstream query_stream;

        pqxx::params params;
        query_stream << "SELECT * FROM " << table;
        uint32_t param_index = 1;
        conditions_to_query(table_name, query_stream, params, param_index, conditions);
        pqxx::result res = execute_query_with_result(query_stream.str(), params);
        results.reserve(res.size());
        for (auto&& row : std::move(res)) {
            auto record = std::make_unique<PqxxViewRecord>();
            record->set_row(std::move(row));
            results.push_back(std::move(record));
        }
        return results;
    }

    Records PqxxClient::select(const std::string_view table_name) const {
        Records results;
        const std::string table = escape_identifier(table_name);
        std::ostringstream query_stream;
        query_stream << "SELECT * FROM " << table;
        const pqxx::result res = execute_query_with_result(query_stream.str());
        results.reserve(res.size());
        for (const auto& row : res) {
            Record record;
            record.reserve(row.size());
            for (const auto& field : row) {
                record.push_back(process_field(field));
            }
            results.push_back(std::move(record));
        }
        return results;
    }

    ViewRecords PqxxClient::view(const std::string_view table_name) const {
        ViewRecords results;
        const std::string table = escape_identifier(table_name);
        std::ostringstream query_stream;
        query_stream << "SELECT * FROM " << table;
        pqxx::result res = execute_query_with_result(query_stream.str());
        results.reserve(res.size());
        for (auto&& row : std::move(res)) {
            auto record = std::make_unique<PqxxViewRecord>();
            record->set_row(std::move(row));
            results.push_back(std::move(record));
        }
        return results;
    }


    void PqxxClient::remove(const std::string_view table_name, const Conditions& conditions) {
        if (conditions.empty()) {
            throw QueryException(
                "Invalid number of conditions. For removing all data call another function", db_err::INVALID_QUERY);
        }
        const std::string table = escape_identifier(table_name);
        std::ostringstream query_stream;
        pqxx::params params;
        query_stream << "DELETE FROM " << table;
        uint32_t param_index = 1;
        conditions_to_query(table_name, query_stream, params, param_index, conditions);

        execute_query(query_stream.str(), params);
    }

    uint32_t PqxxClient::count(const std::string_view table_name) const {
        const std::string table = escape_identifier(table_name);
        std::ostringstream query_stream;
        query_stream << "SELECT COUNT(*) FROM " << table;
        // Execute the query
        const pqxx::result res = execute_query_with_result(query_stream.str());

        return res[0][0].as<uint32_t>();
    }

    uint32_t PqxxClient::count(const std::string_view table_name, const Conditions& conditions) const {
        if (conditions.empty()) {
            throw QueryException(
                "Invalid number of conditions. For count all data call another function", db_err::INVALID_QUERY);
        }
        const std::string table = escape_identifier(table_name);
        std::ostringstream query_stream;
        // Start building the query
        query_stream << "SELECT COUNT(*) FROM " << table;
        // Add conditions if any
        pqxx::params params;
        uint32_t param_index = 1;
        conditions_to_query(table_name, query_stream, params, param_index, conditions);
        const pqxx::result res = execute_query_with_result(query_stream.str(), params);
        return res[0][0].as<uint32_t>();
    }

    void PqxxClient::set_search_fields(const std::string_view table_name, FieldCollection fields) {
        std::lock_guard lock(this->conn_mutex_);
        this->search_fields_[std::string(table_name)] = std::move(fields);
    }

    void PqxxClient::set_conflict_fields(const std::string_view table_name, FieldCollection fields) {
        std::lock_guard lock(this->conn_mutex_);
        this->conflict_fields_[std::string(table_name)] = std::move(fields);
    }

    std::string PqxxClient::make_fts_index_name(const std::string_view table_name) {
        std::ostringstream fts_ind;
        fts_ind << "fts_" << table_name << "_idx";
        return fts_ind.str();
    }

    std::string PqxxClient::make_trgm_index_name(const std::string_view table_name) {
        std::ostringstream fts_ind;
        fts_ind << "trgm_" << table_name << "_idx";
        return fts_ind.str();
    }

    void PqxxClient::install_trgm_extension() const {
        std::ostringstream setup_extensions;
        setup_extensions << "CREATE EXTENSION IF NOT EXISTS pg_trgm;";
        this->execute_query(setup_extensions.str());
    }
} // namespace demiplane::database
