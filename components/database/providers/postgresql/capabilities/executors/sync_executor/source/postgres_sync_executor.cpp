#include "postgres_sync_executor.hpp"

#include "log_macros.hpp"

namespace demiplane::db::postgres {

    gears::Outcome<ResultBlock, ErrorContext> SyncExecutor::execute(const std::string_view query) const {
        // Check connection health
        if (const auto ec = check_connection(conn_); ec) {
            return gears::Err(ErrorContext(ec));
        }

        // Execute simple query
        PGresult* result = PQexec(conn_, query.data());

        // Check if query was sent successfully
        if (!result) {
            return gears::Err(extract_connection_error(conn_));
        }

        return process_result(result);
    }

    gears::Outcome<ResultBlock, ErrorContext> SyncExecutor::execute(const std::string_view query,
                                                                    const Params& params) const {
        COMPONENT_LOG_ENTER_FUNCTION();
        if (const auto ec = check_connection(conn_); ec) {
            ErrorContext ctx(ec);
            COMPONENT_LOG_ERR() << "Connection failed: " << ctx;
            return gears::Err(std::move(ctx));
        }

        // Execute parameterized query
        // Since this is synchronous, libpq will read the data before returning
        PGresult* result = params.values.empty() ? PQexec(conn_, query.data())
                                                 : PQexecParams(conn_,
                                                                query.data(),
                                                                static_cast<int>(params.values.size()),
                                                                params.oids.data(),
                                                                params.values.data(),
                                                                params.lengths.data(),
                                                                params.formats.data(),
                                                                1);  // binary result format

        // Check if query was sent successfully
        if (!result) {
            auto ec = extract_connection_error(conn_);
            COMPONENT_LOG_ERR() << "Connection failed: " << ec;
            return gears::Err(std::move(ec));
        }

        return process_result(result);
    }

    gears::Outcome<ResultBlock, ErrorContext> SyncExecutor::execute(const CompiledQuery& query) const {
        if (query.provider() != SupportedProviders::PostgreSQL) {
            ErrorContext ec{ErrorCode{ClientErrorCode::SyntaxError}};
            ec.context = "Wrong provider. Query was compiled not by PostgreSQL";
            return gears::Err(ec);
        }
        const auto params_ptr = query.backend_packet_as<Params>();
        if (!params_ptr) {
            return execute(query.sql());
        }
        return execute(query.sql(), *params_ptr);
    }

    gears::Outcome<ResultBlock, ErrorContext> SyncExecutor::process_result(PGresult* result) {
        // Extract error if present
        if (auto error_ctx = extract_error(result)) {
            PQclear(result);
            return gears::Err(std::move(*error_ctx));
        }

        // Success - wrap result in ResultBlock (takes ownership)
        return ResultBlock(result);
    }

}  // namespace demiplane::db::postgres
