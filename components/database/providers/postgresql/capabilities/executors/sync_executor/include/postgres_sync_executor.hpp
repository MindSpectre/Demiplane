#pragma once
#include <gears_outcome.hpp>
#include <libpq-fe.h>
#include <postgres_errors.hpp>
#include <postgres_result.hpp>

#include "compiled_query.hpp"
#include "db_error_codes.hpp"
#include "postgres_params.hpp"

namespace demiplane::db::postgres {
    /**
     * @brief Synchronous PostgreSQL query executor
     *
     * Provides blocking query execution using libpq's synchronous API.
     * Uses ErrorContext for rich error information including SQLSTATE,
     * error messages, hints, and context.
     */
    class SyncExecutor {
    public:
        /**
         * @brief Construct a sync executor with a PostgreSQL connection
         * @param conn PostgreSQL connection (must be valid and connected)
         */
        explicit SyncExecutor(PGconn* conn)
            : conn_(conn) {
        }

        /**
         * @brief Execute a simple query without parameters
         * @param query SQL query string
         * @return ResultBlock on success, ErrorContext on failure
         */
        gears::Outcome<ResultBlock, ErrorContext> execute(std::string_view query) const;

        /**
         * @brief Execute a query with parameters
         * @param query SQL query string with placeholders ($1, $2, etc.)
         * @param params Parameter values (moved for efficiency)
         * @return ResultBlock on success, ErrorContext on failure
         */
        gears::Outcome<ResultBlock, ErrorContext> execute(std::string_view query, Params&& params) const;

        /**
         * @brief Execute a compiled query with parameters
         * @param query Compiled query object
         * @param params Parameter values (moved for efficiency)
         * @return ResultBlock on success, ErrorContext on failure
         */
        gears::Outcome<ResultBlock, ErrorContext> execute(const CompiledQuery& query, Params&& params) const;

    private:
        PGconn* conn_;

        /**
         * @brief Process result and check for errors
         * @param result Raw PGresult pointer (takes ownership)
         * @return ResultBlock on success, ErrorContext on error
         */
        static gears::Outcome<ResultBlock, ErrorContext> process_result(PGresult* result);
    };
}  // namespace demiplane::db::postgres
