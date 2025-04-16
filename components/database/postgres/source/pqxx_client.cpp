#include "pqxx_client.hpp"

#include <atomic>

#include "../pqxx_query_engine.hpp"
#include "db_exceptions.hpp"

#define ENABLE_TRACING
#include <demiplane/trace_macros>

using namespace demiplane::database;
using namespace demiplane::database::exceptions;
using db_err = errors::db_error_code;

namespace demiplane::database {
    PqxxClient::PqxxClient(const ConnectParams& connect_params, std::shared_ptr<scroll::Tracer<PqxxClient>> tracer)
        : DbBase(connect_params, std::move(tracer)), in_transaction_(false) {
        try {
            conn_ = std::make_shared<pqxx::connection>(connect_params.make_connect_string());
            if (!conn_->is_open()) {
                TRACER_STREAM_ERROR() << "Failed to open database connection.";
                throw ConnectionException("Failed to open database connection.", db_err::CONNECTION_FAILED);
            }
            TRACE_INFO(get_tracer(), "Connected to database.");
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


    std::unique_ptr<pqxx::work> PqxxClient::initialize_transaction() const {
        if (in_transaction_) {
            // If a long-running transaction is active, return it.
            return std::move(open_transaction_);
        }
        try {
            TRACER_STREAM_DEBUG() << "Transaction has been initialized.";
            return std::make_unique<pqxx::work>(*conn_);
        } catch (const pqxx::broken_connection& e) {
            TRACER_STREAM_FATAL() << e.what();
            throw ConnectionException(e.what(), db_err::CONNECTION_FAILED);
        } catch (std::exception& e) {
            TRACER_STREAM_ERROR() << e.what();
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
            TRACER_STREAM_DEBUG() << "Transaction has been finished successfully.";
        } catch (const pqxx::broken_connection& e) {
            TRACER_STREAM_ERROR() << e.what();
            throw ConnectionException(e.what(), db_err::CONNECTION_FAILED);
        } catch (std::exception& e) {
            TRACER_STREAM_ERROR() << e.what();
            throw TransactionException(
                std::string(e.what()).append(". Undefined exception."), db_err::TRANSACTION_COMMIT_FAILED);
        }
    }
    void PqxxClient::execute_query(const std::string_view query_string, pqxx::params&& params) const {
        std::lock_guard lock(this->conn_mutex_);
        std::unique_ptr<pqxx::work> txn = initialize_transaction();
        txn->exec(pqxx::zview{query_string}, std::move(params));
        finish_transaction(std::move(txn));
    }

    void PqxxClient::execute_query(const std::string_view query_string) const {
        std::lock_guard lock(this->conn_mutex_);
        std::unique_ptr<pqxx::work> txn = initialize_transaction();
        txn->exec(query_string);
        finish_transaction(std::move(txn));
    }

    pqxx::result PqxxClient::execute_query_with_result(
        const std::string_view query_string, pqxx::params&& params) const {
        std::lock_guard lock(this->conn_mutex_);
        std::unique_ptr<pqxx::work> txn = initialize_transaction();
        const pqxx::result response     = txn->exec(pqxx::zview{query_string}, std::move(params));
        finish_transaction(std::move(txn));
        return response;
    }

    pqxx::result PqxxClient::execute_query_with_result(const std::string_view query_string) const {
        std::lock_guard lock(this->conn_mutex_);
        std::unique_ptr<pqxx::work> txn = initialize_transaction();
        const pqxx::result response     = txn->exec(query_string);
        finish_transaction(std::move(txn));
        return response;
    }

    // --- OID Preprocessing and Field Processing ---
    Result PqxxClient::oid_preprocess() {
        Result result;
        result.critical_zone(
            [&] {
                std::unique_lock lock(type_oid_mutex_);
                pqxx::nontransaction txn(*conn_);
                const pqxx::result r = txn.exec(
                    "SELECT typname, oid FROM pg_type WHERE typname IN ('bool', 'int2', 'int4', 'int8', "
                    "'float4', 'float8', 'text', 'varchar', 'bpchar', 'timestamp', 'timestamptz', 'uuid', 'json', "
                    "'jsonb')");
                for (const auto& row : r) {
                    type_oids_.insert({row["oid"].as<uint32_t>(), row["typname"].c_str()});
                }
            },
            [this](const std::exception& e) {
                TRACER_STREAM_ERROR() << "Oids process failed.";
                return analyze_exception(e);
            });
        return result;
    }

    std::unique_ptr<FieldBase> PqxxClient::process_field(const pqxx::field& field) const {
        // Using a factory (assumed to be defined elsewhere) to create the proper Field object.
        // todo: add array support
        using factory = unique_field_factory;
        boost::container::flat_map<uint32_t, std::string>::const_iterator type_oid;
        {
            // todo: rework
            std::shared_lock lock(type_oid_mutex_);
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
            std::string errs;
            std::istringstream json_stream(field.c_str());
            if (Json::CharReaderBuilder reader_builder; !parseFromStream(reader_builder, json_stream, &json, &errs)) {
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
    Result PqxxClient::create_database(const std::shared_ptr<DatabaseConfig>& config, const ConnectParams& pr) {
        Result result;
        result.critical_zone([&] {
            ConnectParams tmp_params = pr;
            tmp_params.set_db_name("template1");
            if (pqxx::connection trivial(tmp_params.make_connect_string()); trivial.is_open()) {
                pqxx::nontransaction nt(trivial);
                std::ostringstream query;
                query << "CREATE DMP_DATABASE " << pr.get_db_name() << " WITH OWNER = " << pr.get_login()
                      << " ENCODING = 'UTF8' TEMPLATE template0;";
                nt.exec(query.str());
                TRACER_STREAM_INFO() << "Database " << pr.get_db_name() << " created successfully!";
            } else {
                TRACER_STREAM_ERROR() << "Connection to database failed.";
            }
        });
        return result;
    }

    Result PqxxClient::connect(const ConnectParams& params) {
        std::lock_guard lock(conn_mutex_);
        // In this design the connection is persistent so we simply update parameters.
        // (Assuming a connection pool is managing reconnections if necessary.)
        // This is a placeholder if you want to support reconnects.
        return Result{};
    }

    Result PqxxClient::drop_connect() {
        std::lock_guard lock(conn_mutex_);
        Result result;
        result.critical_zone(
            [&] {
                conn_->close();
                TRACE_INFO(get_tracer(), "Connection closed.");
            },
            [&](const std::exception& e) {
                TRACE_ERROR(get_tracer(), "Failed to close connect. Required additional diagnostics.");
                return analyze_exception(e);
            });
        return result;
    }

    // --- Transaction Methods ---
    Result PqxxClient::start_transaction() {
        Result result;
        result.capture(
            [&] {
                std::lock_guard lock(conn_mutex_);
                if (in_transaction_) {
                    TRACE_DEBUG(get_tracer(), "Transaction already started.");
                    throw TransactionException("Transaction already started.", db_err::TRANSACTION_START_FAILED);
                }
                open_transaction_ = std::make_unique<pqxx::work>(*conn_);
                in_transaction_   = true;
                TRACE_DEBUG(get_tracer(), "Transaction started.");
            },
            [&](const std::exception& e) {
                TRACE_DEBUG(get_tracer(), "Failed to start transaction.");
                return analyze_exception(e);
            });
        return result;
    }

    Result PqxxClient::commit_transaction() {
        Result result;
        result.capture(
            [&] {
                std::lock_guard lock(conn_mutex_);
                if (!in_transaction_) {
                    TRACE_DEBUG(get_tracer(), "Transaction already committed/rollbacked or not started.");
                    throw TransactionException("No active transaction to commit.", db_err::TRANSACTION_COMMIT_FAILED);
                }
                open_transaction_->commit();
                open_transaction_.reset();
                in_transaction_ = false;
                TRACE_DEBUG(get_tracer(), "Transaction committed.");
            },
            [&](const std::exception& e) {
                TRACE_DEBUG(get_tracer(), "Failed to commit transaction.");
                return analyze_exception(e);
            });

        return result;
    }

    Result PqxxClient::rollback_transaction() {
        Result result;
        result.capture(
            [&] {
                std::lock_guard lock(conn_mutex_);
                if (!in_transaction_) {
                    TRACE_DEBUG(get_tracer(), "Transaction already committed/rolled back or not started.");
                    throw TransactionException(
                        "No active transaction to rollback.", db_err::TRANSACTION_ROLLBACK_FAILED);
                }
                open_transaction_->abort();
                open_transaction_.reset();
                in_transaction_ = false;
                TRACE_DEBUG(get_tracer(), "Transaction rolled back.");
            },
            [&](const std::exception& e) {
                TRACE_DEBUG(get_tracer(), "Failed to rollback transaction.");
                return analyze_exception(e);
            });
        return result;
    }

    // --- Table Management ---
    Result PqxxClient::create_table(const query::CreateTableQuery& query) {
        // Use the processor to generate the CREATE TABLE query.
        const auto qp = query::engine::postgres::process_create(query);
        Result result;
        result.capture(
            [&] {
                execute_query(qp.query);
                TRACER_STREAM_INFO() << "Created table " << query.table() << ".";
            },
            [&](const std::exception& e) {
                TRACER_STREAM_ERROR() << "Failed to create table " << query.table() << ".";
                return analyze_exception(e);
            });
        return result;
    }

    Result PqxxClient::drop_table(const query::DropTableQuery& query) {
        Result result;
        const auto qp = query::engine::postgres::process_drop_table(query);
        result.capture(
            [&] {
                execute_query(qp.query);
                TRACER_STREAM_INFO() << "Table " << query.table() << " deleted.";
            },
            [&](const std::exception& e) {
                TRACER_STREAM_ERROR() << "Failed to delete table " << query.table() << ".";
                return analyze_exception(e);
            });
        return result;
    }

    Result PqxxClient::truncate_table(const query::TruncateTableQuery& query) {
        Result result;
        const auto qp = query::engine::postgres::process_truncate_table(query);
        result.capture(
            [&] {
                execute_query(qp.query);
                TRACER_STREAM_INFO() << "Table " << query.table() << " truncated.";
            },
            [&](const std::exception& e) {
                TRACER_STREAM_ERROR() << "Failed to truncate table " << query.table() << ".";
                return analyze_exception(e);
            });

        return result;
    }

    Interceptor<bool> PqxxClient::check_table(const query::CheckTableQuery& query) {
        Interceptor<bool> result;
        const auto qp = query::engine::postgres::process_check_table(query);
        pqxx::result execute_res;
        result.capture([&] { execute_res = execute_query_with_result(qp.query); },
            [&](const std::exception& e) {
                TRACER_STREAM_ERROR() << "Failed to check table " << query.table() << ".";
                return analyze_exception(e);
            });
        if (result) {
            result.set(!execute_res.empty() && !execute_res[0][0].is_null());
        }

        return result;
    }
    Result PqxxClient::set_unique_constraint(const query::SetUniqueConstraint& query) {
        Result result;
        const auto qp = query::engine::postgres::process_set_unique_constraint(query);
        result.capture([&] {
            execute_query(qp.query);
            TRACER_STREAM_INFO() << "Set unique constraint for table:" << query.table() << "On these columns::" << ".";
        });
        return result;
    }
    Result PqxxClient::delete_unique_constraint(const query::DeleteUniqueConstraint& table_name) {
        Result result;
        auto qp = query::engine::postgres::process_delete_unique_constraint(table_name);
        return result;
    }
    inline Interceptor<std::optional<Records>> PqxxClient::insert(query::InsertQuery query) {

        const bool is_returning = query.has_returning_fields();
        auto qp                 = query::engine::postgres::process_insert(std::move(query));
        Interceptor<std::optional<Records>> result;
        pqxx::result execute_res;
        result.capture(
            [&] {
                execute_res = execute_query_with_result(qp.query, std::move(qp.params));
                TRACER_STREAM_DEBUG() << "Inserting records for table:" << query.table() << "";
            },
            [this](const std::exception& e) { return analyze_exception(e); });
        if (result && is_returning) {
            Records records;
            records.reserve(execute_res.size());
            for (const auto& row : execute_res) {
                Record record;
                record.reserve(row.size());
                for (const auto& field : row) {
                    record.push_back(process_field(field));
                }
                records.push_back(std::move(record));
            }
            result.set(std::move(records));
        }
        return result;
    }
    inline Interceptor<std::optional<Records>> PqxxClient::upsert(query::UpsertQuery&& query) {
        const bool is_returning = query.has_returning_fields();
        auto qp                 = query::engine::postgres::process_upsert(std::move(query));
        Interceptor<std::optional<Records>> result;
        pqxx::result execute_res;
        result.capture([&] { execute_res = execute_query_with_result(qp.query, std::move(qp.params)); });
        if (result && is_returning) {
            auto& records = result.ref();
            records->reserve(execute_res.size());
            for (const auto& row : execute_res) {
                Record record;
                for (const auto& field : row) {
                    record.push_back(process_field(field));
                }
                records->push_back(std::move(record));
            }
        }
        return result;
    }
    inline Interceptor<Records> PqxxClient::select(const query::SelectQuery& query) const {
        auto qp = query::engine::postgres::process_select(query);
        Interceptor<Records> result;
        pqxx::result execute_res;
        result.capture(
            [&] {
                execute_res = execute_query_with_result(qp.query, std::move(qp.params));
                TRACER_STREAM_DEBUG() << "Selecting records for table:" << query.table() << "";
            },
            [this](const std::exception& e) { return analyze_exception(e); });
        if (result) {
            auto& records = result.ref();
            records.reserve(execute_res.size());
            for (const auto& row : execute_res) {
                Record record;
                for (const auto& field : row) {
                    record.push_back(process_field(field));
                }
                records.push_back(std::move(record));
            }
        }
        return result;
    }
    inline Interceptor<std::optional<Records>> PqxxClient::remove(const query::RemoveQuery& query) {
        const bool is_returning = query.has_returning_fields();
        auto qp                 = query::engine::postgres::process_remove(query);
        Interceptor<std::optional<Records>> result;
        pqxx::result execute_res;
        result.capture([&] { execute_res = execute_query_with_result(qp.query, std::move(qp.params)); });
        if (result && is_returning) {
            auto& records = result.ref();
            records->reserve(execute_res.size());
            for (const auto& row : execute_res) {
                Record record;
                for (const auto& field : row) {
                    record.push_back(process_field(field));
                }
                records->push_back(std::move(record));
            }
        }
        return result;
    }
    Interceptor<uint32_t> PqxxClient::count(const query::CountQuery& query) const {
        auto qp = query::engine::postgres::process_count(query);
        Interceptor<uint32_t> result;
        pqxx::result execute_res;
        result.capture([&] { execute_res = execute_query_with_result(qp.query, std::move(qp.params)); },
            [&](const std::exception& e) { return analyze_exception(e); });
        if (result) {
            result.set(execute_res[0][0].as<uint32_t>());
        }
        return result;
    }
    Result PqxxClient::setup_search_index(const query::SetIndexQuery& query) {
        Result result;
        std::queue<PostgresRequest> queue_qp = query::engine::postgres::process_set_search_index(query, configuration_);
        result.capture(
            [&] {
                start_transaction();
                while (!queue_qp.empty()) {
                    auto qp = std::move(queue_qp.front());
                    queue_qp.pop();
                    execute_query(qp.query, std::move(qp.params));
                }
                commit_transaction();
            },
            [&](const std::exception& e) { return analyze_exception(e); });
        return result;
    }
    Result PqxxClient::drop_search_index(const query::DropIndexQuery& query) {
        Result result;
        std::queue<PostgresRequest> queue_qp =
            query::engine::postgres::process_drop_search_index(query, configuration_);
        result.capture(
            [&] {
                start_transaction();
                while (!queue_qp.empty()) {
                    auto qp = std::move(queue_qp.front());
                    queue_qp.pop();
                    execute_query(qp.query, std::move(qp.params));
                }
                commit_transaction();
            },
            [&](const std::exception& e) {
                TRACER_STREAM_ERROR() << "Failed to drop search index for table " << query.table() << ".";
                return analyze_exception(e);
            });
        return result;
    }

    std::exception_ptr PqxxClient::analyze_exception(const std::exception& caught_exception) const {
        TRACER_STREAM_ERROR() << "\nERROR msg: " << caught_exception.what();
        return std::make_exception_ptr(caught_exception);
    }


} // namespace demiplane::database
