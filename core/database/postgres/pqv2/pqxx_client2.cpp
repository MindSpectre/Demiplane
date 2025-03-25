#include "pqxx_client.hpp"
using namespace demiplane::database;
using namespace demiplane::database::exceptions;
using db_err = errors::db_error_code;
namespace demiplane::database {
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

    std::string PqxxClient::escape_identifier(const std::string_view identifier) const {
        if (!is_valid_identifier(identifier)) {
            throw InvalidIdentifierException(std::string(identifier), db_err::INVALID_QUERY);
        }
        return conn_->quote_name(std::string(identifier));
    }

    std::unique_ptr<pqxx::work> PqxxClient::initialize_transaction() const {
        if (in_transaction_) {
            // If a long-running transaction is active, return it.
            return std::move(open_transaction_);
        }
        try {
            TRACER_STREAM_INFO(tracer_) << "Transaction has been initialized." << std::endl;
            return std::make_unique<pqxx::work>(*conn_);
        } catch (const pqxx::broken_connection& e) {
            TRACER_STREAM_ERROR(tracer_) << e.what() << std::endl;
            throw ConnectionException(e.what(), db_err::CONNECTION_FAILED);
        } catch (std::exception& e) {
            TRACER_STREAM_ERROR(tracer_) << e.what() << std::endl;
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
            TRACER_STREAM_INFO(tracer_) << "Transaction has been finished succesfully." << std::endl;
        } catch (const pqxx::broken_connection& e) {
            TRACER_STREAM_ERROR(tracer_) << e.what() << std::endl;
            throw ConnectionException(e.what(), db_err::CONNECTION_FAILED);
        } catch (std::exception& e) {
            TRACER_STREAM_ERROR(tracer_) << e.what() << std::endl;
            throw TransactionException(
                std::string(e.what()).append(". Undefined exception."), db_err::TRANSACTION_COMMIT_FAILED);
        }
    }
    void PqxxClient::execute_query(const std::string& query_string, pqxx::params&& params) const {
        std::lock_guard lock(this->conn_mutex_);
        try {
            std::unique_ptr<pqxx::work> txn = initialize_transaction();
            txn->exec_params(query_string, std::move(params));
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

    pqxx::result PqxxClient::execute_query_with_result(const std::string& query_string, pqxx::params&& params) const {
        std::lock_guard lock(this->conn_mutex_);
        try {
            std::unique_ptr<pqxx::work> txn = initialize_transaction();
            const pqxx::result response     = txn->exec_params(query_string, std::move(params));
            finish_transaction(std::move(txn));
            return response;
        } catch (const std::exception& e) {
            throw adapt_exception(e);
        }
    }

    pqxx::result PqxxClient::execute_query_with_result(const std::string& query_string) const {

        try {
            std::unique_ptr<pqxx::work> txn = initialize_transaction();
            const pqxx::result response     = txn->exec_params(query_string);
            finish_transaction(std::move(txn));
            return response;
        } catch (const std::exception& e) {
            throw adapt_exception(e);
        }
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
        // todo: add array support
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
    IRes<> PqxxClient::create_database(const std::shared_ptr<DatabaseConfig>& config, const ConnectParams& pr) {
        IRes<> result;
        result.critical_zone([&] {
            if (pqxx::connection trivial(pr.make_connect_string()); trivial.is_open()) {
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
        result.critical_zone(
            [&] {
                conn_->close();
                TRACE_INFO(tracer_, "Connection closed.");
            },
            [&] { TRACE_ERROR(tracer_, "Failed to close connect. Required additional diagnostics."); });
        return result;
    }

    // --- Transaction Methods ---
    IRes<> PqxxClient::start_transaction() {
        IRes<> result;
        result.capture([&] {
            if (in_transaction_) {
                TRACE_ERROR(tracer_, "Transaction already started.");
                throw TransactionException("Transaction already started.", db_err::TRANSACTION_START_FAILED);
            }
            std::lock_guard lock(conn_mutex_);
            open_transaction_ = std::make_unique<pqxx::work>(*conn_);
            in_transaction_   = true;
            TRACE_INFO(tracer_, "Transaction started.");
        }, [&]{TRACE_ERROR(tracer_, "Failed to start transaction."); });
        return result;
    }

    IRes<> PqxxClient::commit_transaction() {
        IRes<> result;
        result.capture([&] {
            if (!in_transaction_) {
                TRACE_ERROR(tracer_, "Transaction already committed/rollbacked or not started.");
                throw TransactionException("No active transaction to commit.", db_err::TRANSACTION_COMMIT_FAILED);
            }
            std::lock_guard lock(conn_mutex_);
            open_transaction_->commit();
            open_transaction_.reset();
            in_transaction_ = false;
            TRACE_INFO(tracer_, "Transaction committed.");
        }, [&]{TRACE_ERROR(tracer_, "Failed to commit transaction."); });

        return result;
    }

    IRes<> PqxxClient::rollback_transaction() {
        IRes<> result;
        result.capture([&] {
            if (!in_transaction_) {
                TRACE_ERROR(tracer_, "Transaction already committed/rolled back or not started.");
                throw TransactionException("No active transaction to rollback.", db_err::TRANSACTION_ROLLBACK_FAILED);
            }
            std::lock_guard lock(conn_mutex_);
            open_transaction_->abort();
            open_transaction_.reset();
            in_transaction_ = false;
            TRACE_INFO(tracer_, "Transaction rolled back.");
        }, [&]{TRACE_ERROR(tracer_, "Failed to rollback transaction."); });
        return result;
    }

    // --- Table Management ---
    IRes<> PqxxClient::create_table(const query::CreateQuery& proposal) {
        // Use the processor to generate the CREATE TABLE query.
        auto qp = PqxxQueryEngine::process(proposal);
        IRes<> result;
        result.capture([&] { execute_query(qp.query, std::move(qp.params)); });
        return result;
    }

    IRes<> PqxxClient::delete_table(std::string_view table_name) {
        IRes<> result;
        std::ostringstream oss;
        oss << "DROP TABLE IF EXISTS " << escape_identifier(table_name) << ";";
        result.capture([&] { execute_query(oss.str()); });
        return result;
    }

    IRes<> PqxxClient::truncate_table(std::string_view table_name) {
        IRes<> result;
        std::ostringstream oss;
        oss << "TRUNCATE TABLE " << escape_identifier(table_name) << ";";
        result.capture([&] { execute_query(oss.str()); });

        return result;
    }

    IRes<bool> PqxxClient::check_table(std::string_view table_name) {
        IRes<bool> result;
        std::ostringstream oss;
        oss << "SELECT to_regclass(" << conn_->quote(std::string(table_name)) << ");";
        pqxx::result execute_res;
        result.capture([&] { execute_res = execute_query_with_result(oss.str()); });
        if (result.is_ok()) {
            result.set(!execute_res.empty() && !execute_res[0][0].is_null());
        }

        return result;
    }

    IRes<> PqxxClient::make_unique_constraint(std::string_view table_name, FieldCollection key_fields) {
        IRes<> result;
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
        result.capture([&] { execute_query(query); });
        return result;
    }
    inline IRes<std::optional<Records>> PqxxClient::insert(query::InsertQuery&& query) {}
    inline IRes<std::optional<Records>> PqxxClient::upsert(query::UpsertQuery&& query) {}
    inline IRes<Records> PqxxClient::select(const query::SelectQuery& conditions) const {}
    inline IRes<std::optional<Records>> PqxxClient::remove(const query::DeleteQuery& conditions) {}
    inline IRes<uint32_t> PqxxClient::count(const query::CountQuery& conditions) const {}
    inline exceptions::DatabaseException PqxxClient::adapt_exception(const std::exception& pqxx_exception) {}
    inline void PqxxClient::build_conflict_clause_for_force_insert(
        std::string& query, std::string_view table_name, const FieldCollection& replace_fields) const {}
    inline void PqxxClient::build_returning_clause(std::string& query, const FieldCollection& returning_fields) {}
    inline void PqxxClient::create_fts_index_query(std::string_view table_name, std::ostringstream& index_query) const {
    }
    inline void PqxxClient::create_trgm_index_query(
        std::string_view table_name, std::ostringstream& index_query) const {}
    inline std::string PqxxClient::make_fts_index_name(std::string_view table_name) {}
    inline std::string PqxxClient::make_trgm_index_name(std::string_view table_name) {}
    inline void PqxxClient::install_trgm_extension() const {}
} // namespace demiplane::database
