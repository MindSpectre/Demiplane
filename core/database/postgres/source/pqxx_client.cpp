#include "pqxx_client.hpp"

#include <chrono>
#include <exception>
#include <iomanip>
#include <iostream>
#include <regex>
#include <sstream>

#include "db_interface.hpp"
#include "stopwatch.hpp"

namespace demiplane::database {
    using namespace exceptions;
    using db_err = errors::db_error_code;

    // --- Constructor ---
    PqxxClient::PqxxClient(const ConnectParams& pr) : DbInterface(pr), in_transaction_(false) {
        tracer_ = scroll::TracerFactory::create_default_console_tracer<PqxxClient>();
        try {
            conn_ = std::make_shared<pqxx::connection>(pr.make_connect_string());
            if (!conn_->is_open()) {
                TRACER_STREAM_ERROR(tracer_) << "Failed to open database connection." << std::endl;
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

    // --- Utility Functions ---
    bool PqxxClient::is_valid_identifier(std::string_view identifier) {
        static const std::regex valid_identifier_regex("^[a-zA-Z_][a-zA-Z0-9_]*$");
        return std::regex_match(identifier.begin(), identifier.end(), valid_identifier_regex);
    }

    std::string PqxxClient::escape_identifier(std::string_view identifier) const {
        if (!is_valid_identifier(identifier)) {
            throw InvalidIdentifierException(std::string(identifier), db_err::INVALID_QUERY);
        }
        // Use PQXX's quote_name to properly escape the identifier.
        return conn_->quote_name(std::string(identifier));
    }

    // --- Transaction Helpers ---
    std::unique_ptr<pqxx::work> PqxxClient::initialize_transaction() const {
        if (in_transaction_) {
            // If a long-running transaction is active, return it.
            return std::move(open_transaction_);
        }
        try {
            return std::make_unique<pqxx::work>(*conn_);
        } catch (const pqxx::broken_connection& e) {
            throw ConnectionException(e.what(), db_err::CONNECTION_FAILED);
        } catch (std::exception& e) {
            throw TransactionException(
                std::string(e.what()).append(". Undefined exception."), db_err::TRANSACTION_START_FAILED);
        }
    }

    void PqxxClient::finish_transaction(std::unique_ptr<pqxx::work>&& current_transaction) const {
        if (in_transaction_) {
            // If inside a long-running transaction, simply update the pointer.
            open_transaction_ = std::move(current_transaction);
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
            throw pqxx_exception;
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
            throw;
        }
    }

    // --- Execution Helpers ---
    IRes<> PqxxClient::execute_query(const std::string& query_string, const pqxx::params& params) const {
        std::lock_guard lock(conn_mutex_);
        IRes<> result;
        result.capture([&] {
            auto txn = initialize_transaction();
            txn->exec_params(query_string, params);
            finish_transaction(std::move(txn));
        });
        return result;
    }

    IRes<> PqxxClient::execute_query(const std::string& query_string) const {
        std::lock_guard lock(conn_mutex_);
        IRes<> result;
        result.capture([&] {
            auto txn = initialize_transaction();
            txn->exec(query_string);
            finish_transaction(std::move(txn));
        });
        return result;
    }

    IRes<pqxx::result> PqxxClient::execute_query_with_result(
        const std::string& query_string, const pqxx::params& params) const {
        std::lock_guard lock(conn_mutex_);
        IRes<pqxx::result> result;
        result.capture([&] {
            auto txn              = initialize_transaction();
            pqxx::result response = txn->exec_params(query_string, params);
            finish_transaction(std::move(txn));
            result.set(std::move(response));
        });
        return result;
    }

    IRes<pqxx::result> PqxxClient::execute_query_with_result(const std::string& query_string) const {
        std::lock_guard lock(conn_mutex_);
        IRes<pqxx::result> result;
        result.capture([&] {
            auto txn              = initialize_transaction();
            pqxx::result response = txn->exec(query_string);
            finish_transaction(std::move(txn));
            result.set(std::move(response));
        });
        return result;
    }

    // --- OID Preprocessing and Field Processing ---
    IRes<> PqxxClient::oid_preprocess() {
        IRes<> result;
        result.critical_zone([&] {
            std::lock_guard lock(conn_mutex_);
            pqxx::nontransaction txn(*conn_);
            const pqxx::result r =
                txn.exec("SELECT typname, oid FROM pg_type WHERE typname IN ('bool', 'int2', 'int4', 'int8', "
                         "'float4', 'float8', 'text', 'varchar', 'bpchar', 'timestamp', 'timestamptz', 'uuid', 'json', "
                         "'jsonb')");
            for (const auto& row : r) {
                type_oids_.insert({row["oid"].as<uint32_t>(), row["typname"].c_str()});
            }
        });
        return result;
    }

    std::unique_ptr<FieldBase> PqxxClient::process_field(const pqxx::field& field) const {
        // Using a factory (assumed to be defined elsewhere) to create the proper Field object.
        using factory = unique_field_factory;
        boost::container::flat_map<uint32_t, std::string>::const_iterator type_oid;
        {
            std::lock_guard lock(conn_mutex_);
            type_oid = type_oids_.find(field.type());
            if (type_oid == type_oids_.end()) {
                throw std::invalid_argument("Field type not found");
            }
        }
        if (type_oid->second == "int4") {
            return factory::int_field(field.name(), field.as<int32_t>());
        }
        if (type_oid->second == "text") {
            return factory::text_field(field.name(), field.as<std::string>());
        }
        if (type_oid->second == "int8") {
            return factory::ll_int_field(field.name(), field.as<int64_t>());
        }
        if (type_oid->second == "jsonb" || type_oid->second == "json") {
            Json::Value json;
            Json::CharReaderBuilder reader_builder;
            std::string errs;
            std::istringstream json_stream(field.c_str());
            if (!parseFromStream(reader_builder, json_stream, &json, &errs)) {
                throw std::runtime_error("Failed to parse JSON: " + errs);
            }
            return factory::json_field(field.name(), json);
        }
        if (type_oid->second == "float8") {
            return factory::double_field(field.name(), field.as<double>());
        }
        if (type_oid->second == "bool") {
            return factory::bool_field(field.name(), field.as<bool>());
        }
        if (type_oid->second == "uuid") {
            if (field.is_null()) {
                return factory::uuid_field(field.name(), Uuid().set_null());
            }
            return factory::uuid_field(field.name(), Uuid(field.as<std::string>(), false));
        }
        if (type_oid->second == "timestamptz" || type_oid->second == "timestamp") {
            auto value = field.as<std::string>();
            std::istringstream iss(value);
            std::tm tm = {};
            iss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
            auto tp = std::chrono::system_clock::from_time_t(std::mktime(&tm));
            return factory::time_field(field.name(), tp);
        }
        return nullptr;
    }

    // --- Conflict and Returning Clause Builders ---
    void PqxxClient::build_conflict_clause_for_force_insert(
        std::string& query, std::string_view table_name, const FieldCollection& replace_fields) const {
        std::ostringstream conflict_stream;
        FieldCollection conflict_fields_local;
        {
            std::lock_guard lock(conn_mutex_);
            try {
                conflict_fields_local = conflict_fields_.at(std::string(table_name));
            } catch (const boost::container::out_of_range&) {
                throw QueryException("For this table upsert clause is not set up. Invalid table name credentials.",
                    db_err::INVALID_DATA);
            }
        }
        conflict_stream << " ON CONFLICT (";
        for (const auto& field : conflict_fields_local) {
            conflict_stream << escape_identifier(field->get_name()) << ", ";
        }
        std::string conflict_clause = conflict_stream.str();
        conflict_clause.erase(conflict_clause.size() - 2);
        conflict_clause += ") DO UPDATE SET ";
        for (const auto& field : replace_fields) {
            std::string name = escape_identifier(field->get_name());
            conflict_clause.append(name).append(" = EXCLUDED.").append(name).append(", ");
        }
        conflict_clause.erase(conflict_clause.size() - 2);
        query += conflict_clause;
    }

    void PqxxClient::build_returning_clause(std::string& query, const FieldCollection& returning_fields) {
        std::ostringstream ret_stream;
        ret_stream << " RETURNING ";
        for (const auto& field : returning_fields) {
            ret_stream << escape_identifier(field->get_name()) << ", ";
        }
        std::string ret_clause = ret_stream.str();
        ret_clause.erase(ret_clause.size() - 2);
        query += ret_clause;
    }

    // --- FTS and Trigram Index Helpers ---
    std::string PqxxClient::make_fts_index_name(std::string_view table_name) {
        std::ostringstream fts_ind;
        fts_ind << "fts_" << table_name << "_idx";
        return fts_ind.str();
    }

    std::string PqxxClient::make_trgm_index_name(std::string_view table_name) {
        std::ostringstream trgm_ind;
        trgm_ind << "trgm_" << table_name << "_idx";
        return trgm_ind.str();
    }

    void PqxxClient::create_fts_index_query(std::string_view table_name, std::ostringstream& index_query) const {
        const std::string table = escape_identifier(table_name);
        std::ostringstream fields_stream;
        FieldCollection fts_fields;
        {
            std::lock_guard lock(conn_mutex_);
            try {
                fts_fields = search_fields_.at(std::string(table_name));
            } catch (const std::out_of_range&) {
                throw QueryException("FTS fields not set up for table.", db_err::INVALID_DATA);
            }
        }
        for (const auto& field : fts_fields) {
            fields_stream << "coalesce(" << escape_identifier(field->get_name()) << "::text, '') || ' ' || ";
        }
        std::string concatenated = fields_stream.str();
        concatenated.erase(concatenated.size() - 11);
        index_query << "CREATE INDEX IF NOT EXISTS " << make_fts_index_name(table_name) << " ON " << table
                    << " USING gin (to_tsvector('simple', " << concatenated << "));";
    }

    void PqxxClient::create_trgm_index_query(std::string_view table_name, std::ostringstream& index_query) const {
        const std::string table = escape_identifier(table_name);
        std::ostringstream fields_stream;
        FieldCollection trgm_fields;
        {
            std::lock_guard lock(conn_mutex_);
            try {
                trgm_fields = search_fields_.at(std::string(table_name));
            } catch (const std::out_of_range&) {
                throw QueryException("Trigram fields not set up for table.", db_err::INVALID_DATA);
            }
        }
        for (const auto& field : trgm_fields) {
            fields_stream << "coalesce(" << escape_identifier(field->get_name()) << "::text, '') || ' ' || ";
        }
        std::string concatenated = fields_stream.str();
        concatenated.erase(concatenated.size() - 11);
        index_query << "CREATE INDEX IF NOT EXISTS " << make_trgm_index_name(table_name) << " ON " << table
                    << " USING gin ((" << concatenated << ") gin_trgm_ops);";
    }

    // IRes<> PqxxClient::install_trgm_extension() const {
    //     std::ostringstream setup;
    //     setup << "CREATE EXTENSION IF NOT EXISTS pg_trgm;";
    //     execute_query(setup.str());
    // }

    // --- Connection Management ---
    IRes<> PqxxClient::create_database(const std::shared_ptr<DatabaseConfig>& config, const ConnectParams& pr) {
        IRes<> result;
        result.critical_zone([&] {
            pqxx::connection trivial(pr.make_connect_string());
            if (trivial.is_open()) {
                pqxx::nontransaction nt(trivial);
                std::ostringstream query;
                query << "CREATE DATABASE " << pr.get_db_name() << " WITH OWNER = " << pr.get_login()
                      << " ENCODING = 'UTF8' TEMPLATE template0;";
                nt.exec(query.str());
                nt.commit();
                TRACER_STREAM_INFO(tracer_) << "Database " << pr.get_db_name() << " created successfully!" << std::endl;
            } else {
                TRACER_STREAM_ERROR(tracer_) << "Connection to database failed." << std::endl;
            }
        });
        return result;
    }

    IRes<> PqxxClient::connect(const ConnectParams& params) {
        std::lock_guard lock(conn_mutex_);
        // In this design the connection is persistent so we simply update parameters.
        // (Assuming a connection pool is managing reconnections if necessary.)
        // This is a placeholder if you want to support reconnects.
        return IRes<>::sOk();
    }

    IRes<> PqxxClient::drop_connect() {
        std::lock_guard lock(conn_mutex_);
        IRes<> result;
        result.critical_zone([&] { conn_->close(); });
        return result;
    }

    // --- Transaction Methods ---
    IRes<> PqxxClient::start_transaction() {
        if (in_transaction_) {
            throw TransactionException("Transaction already started.", db_err::TRANSACTION_START_FAILED);
        }
        IRes<> result;
        result.capture([&] {
            std::lock_guard lock(conn_mutex_);
            open_transaction_ = std::make_unique<pqxx::work>(*conn_);
            in_transaction_   = true;
        });
        return result;
    }

    IRes<> PqxxClient::commit_transaction() {
        if (!in_transaction_) {
            throw TransactionException("No active transaction to commit.", db_err::TRANSACTION_COMMIT_FAILED);
        }
        std::lock_guard lock(conn_mutex_);
        open_transaction_->commit();
        open_transaction_.reset();
        in_transaction_ = false;
    }

    IRes<> PqxxClient::rollback_transaction() {
        if (!in_transaction_) {
            throw TransactionException("No active transaction to rollback.", db_err::TRANSACTION_ROLLBACK_FAILED);
        }
        std::lock_guard lock(conn_mutex_);
        open_transaction_->abort();
        open_transaction_.reset();
        in_transaction_ = false;
    }

    // --- Table Management ---
    IRes<> PqxxClient::create_table(const query::CreateQuery& proposal) {
        // Use the processor to generate the CREATE TABLE query.
        auto qp = PqxxQueryProcessor::process(proposal);
        if (qp.has_captured()) {

        }
        execute_query(qp.response().query, qp.response().params);
    }

    IRes<> PqxxClient::delete_table(std::string_view table_name) {
        std::ostringstream oss;
        oss << "DROP TABLE IF EXISTS " << escape_identifier(table_name) << ";";
        execute_query(oss.str());
    }

    IRes<> PqxxClient::truncate_table(std::string_view table_name) {
        std::ostringstream oss;
        oss << "TRUNCATE TABLE " << escape_identifier(table_name) << ";";
        execute_query(oss.str());
    }

    IRes<bool> PqxxClient::check_table(std::string_view table_name) {
        std::ostringstream oss;
        oss << "SELECT to_regclass(" << conn_->quote(std::string(table_name)) << ");";
        auto execute_res = execute_query_with_result(oss.str());
        if (execute_res.is_err()) {
            return IRes<bool>{execute_res};
        }
        IRes<bool> result;
        const auto pq_result = execute_res.response();
        result.set(!pq_result.empty() && !pq_result[0][0].is_null());
        return result;
    }

    IRes<> PqxxClient::make_unique_constraint(std::string_view table_name, FieldCollection key_fields) {
        std::lock_guard lock(conn_mutex_);
        conflict_fields_[std::string(table_name)] = std::move(key_fields);
        // Build and execute the unique constraint query.
        std::ostringstream query_stream;
        query_stream << "ALTER TABLE " << escape_identifier(table_name) << " ADD CONSTRAINT ";
        std::ostringstream name_stream, sub_stream;
        for (const auto& field : conflict_fields_.at(std::string(table_name))) {
            sub_stream << escape_identifier(field->get_name()) << ", ";
            name_stream << field->get_name() << "_";
        }
        std::string constraint_name = escape_identifier(name_stream.str() + std::string(table_name));
        std::string columns         = sub_stream.str();
        columns.erase(columns.size() - 2);
        std::string query = "ALTER TABLE " + escape_identifier(table_name) + " ADD CONSTRAINT " + constraint_name
                          + " UNIQUE (" + columns + ");";
        execute_query(query);
    }

    IRes<> PqxxClient::setup_search_index(std::string_view table_name, FieldCollection fields) {
        if (fields.empty()) {
            throw QueryException("Expected at least one FTS field.", db_err::INVALID_QUERY);
        }
        {
            std::lock_guard lock(conn_mutex_);
            search_fields_[std::string(table_name)] = std::move(fields);
        }
        try {
            std::ostringstream fts_query;
            create_fts_index_query(table_name, fts_query);
            execute_query(fts_query.str());
            std::ostringstream trgm_query;
            create_trgm_index_query(table_name, trgm_query);
            try {
                execute_query(trgm_query.str());
            } catch (const std::exception& e) {
                install_trgm_extension();
                execute_query(trgm_query.str());
            }
        } catch (const std::exception& e) {
            throw adapt_exception(e);
        }
    }

    IRes<> PqxxClient::drop_search_index(std::string_view table_name) const {
        std::ostringstream oss;
        oss << "DROP INDEX IF EXISTS " << make_fts_index_name(table_name) << ";";
        execute_query(oss.str());
        std::ostringstream oss2;
        oss2 << "DROP INDEX IF EXISTS " << make_trgm_index_name(table_name) << ";";
        execute_query(oss2.str());
    }

    IRes<> PqxxClient::remove_search_index(std::string_view table_name) {
        std::lock_guard lock(conn_mutex_);
        search_fields_[std::string(table_name)].clear();
        drop_search_index(table_name);
    }

    IRes<> PqxxClient::restore_search_index(std::string_view table_name) const {
        try {
            std::ostringstream fts_query;
            create_fts_index_query(table_name, fts_query);
            execute_query(fts_query.str());
            std::ostringstream trgm_query;
            create_trgm_index_query(table_name, trgm_query);
            execute_query(trgm_query.str());
        } catch (const QueryException& e) {
            if (e.get_error() == errors::db_error_code::INVALID_DATA) {
                throw QueryException(
                    "FTS fields were not set up. Use setup_search_index.", errors::db_error_code::INVALID_DATA);
            }
            throw adapt_exception(e);
        } catch (const std::exception& e) {
            throw adapt_exception(e);
        }
    }

    // --- Data Manipulation using Query Processor ---
    IRes<> PqxxClient::insert(query::InsertQuery&& query) {
        auto qp = PqxxQueryProcessor::process(query);
        if (qp.has_captured()) {
            qp.rethrow();
        }
        execute_query(qp.response.query, qp.response.params);
    }

    IRes<> PqxxClient::upsert(query::UpsertQuery&& query) {
        auto qp = PqxxQueryProcessor::process(query);
        if (qp.has_captured()) {
            qp.rethrow();
        }
        execute_query(qp.response.query, qp.response.params);
    }

    IRes<Records> PqxxClient::insert_with_returning(query::InsertQuery&& query) {
        auto qp = PqxxQueryProcessor::process(query);
        if (qp.has_captured()) {
            qp.rethrow();
        }
        Records results;
        const pqxx::result res = execute_query_with_result(qp.response.query, qp.response.params);
        for (const auto& row : res) {
            Record record;
            for (const auto& field : row) {
                record.push_back(process_field(field));
            }
            results.push_back(std::move(record));
        }
        return results;
    }

    IRes<Records> PqxxClient::upsert_with_returning(query::UpsertQuery&& query) {
        auto qp = PqxxQueryProcessor::process(query);
        if (qp.has_captured()) {
            qp.rethrow();
        }
        Records results;
        const pqxx::result res = execute_query_with_result(qp.response.query, qp.response.params);
        for (const auto& row : res) {
            Record record;
            for (const auto& field : row) {
                record.push_back(process_field(field));
            }
            results.push_back(std::move(record));
        }
        return results;
    }

    IRes<Records> PqxxClient::select(const query::SelectQuery& conditions) const {
        auto qp = PqxxQueryProcessor::process(conditions);
        if (qp.has_captured()) {
            qp.rethrow();
        }
        Records results;
        const pqxx::result res = execute_query_with_result(qp.response.query, qp.response.params);
        for (const auto& row : res) {
            Record record;
            for (const auto& field : row) {
                record.push_back(process_field(field));
            }
            results.push_back(std::move(record));
        }
        return results;
    }

    IRes<> PqxxClient::remove(const query::DeleteQuery& conditions) {
        auto qp = PqxxQueryProcessor::process(conditions);
        if (qp.has_captured()) {
            qp.rethrow();
        }
        execute_query(qp.response.query, qp.response.params);
    }

    IRes<uint32_t> PqxxClient::count(const query::CountQuery& conditions) const {
        auto qp = PqxxQueryProcessor::process(conditions);
        if (qp.has_captured()) {
            qp.rethrow();
        }
        const pqxx::result res = execute_query_with_result(qp.response.query, qp.response.params);
        return res[0][0].as<uint32_t>();
    }

    // --- Setting Search and Conflict Fields ---
    void PqxxClient::set_search_fields(std::string_view table_name, FieldCollection fields) {
        std::lock_guard lock(conn_mutex_);
        search_fields_[std::string(table_name)] = std::move(fields);
    }

    void PqxxClient::set_conflict_fields(std::string_view table_name, FieldCollection fields) {
        std::lock_guard lock(conn_mutex_);
        conflict_fields_[std::string(table_name)] = std::move(fields);
    }
} // namespace demiplane::database
