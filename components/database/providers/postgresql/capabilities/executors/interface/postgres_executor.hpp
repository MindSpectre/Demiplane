#pragma once

#include <gears_outcome.hpp>
#include <postgres_errors.hpp>
#include <postgres_result.hpp>

namespace demiplane::db::postgres {
    using DatabaseResult = gears::Outcome<ResultBlock, ErrorContext>;

    /**
     * @brief Process PostgreSQL result and check for errors
     * @param result Raw PGresult pointer (takes ownership)
     * @return ResultBlock on success, ErrorContext on error
     *
     * This is a common helper used by both sync and async executors
     * to process PGresult objects and handle errors consistently.
     */
    inline gears::Outcome<ResultBlock, ErrorContext> process_result(PGresult* result) {
        // Extract error if present
        if (auto error_ctx = extract_error(result)) {
            PQclear(result);
            return gears::Err(std::move(*error_ctx));
        }

        // Success - wrap result in ResultBlock (takes ownership)
        return ResultBlock(result);
    }
}  // namespace demiplane::db::postgres
