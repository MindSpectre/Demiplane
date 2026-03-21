#pragma once

// ============== ExecStatusType → ErrorCode Mapping ==============
//
// Maps libpq ExecStatusType to our ErrorCode system.
// Used as a coarse fallback when SQLSTATE is unavailable.
//
// Included by postgres_errors.cpp — not a standalone translation unit.

#include <optional>

#include <db_error_codes.hpp>
#include <libpq-fe.h>

namespace demiplane::db::postgres::detail {

    [[nodiscard]] inline std::optional<ErrorCode> map_exec_status(const ExecStatusType status) noexcept {
        switch (status) {
            case PGRES_EMPTY_QUERY:
                return ErrorCode(ClientErrorCode::InvalidArgument);

            case PGRES_COMMAND_OK:
            case PGRES_TUPLES_OK:
            case PGRES_COPY_OUT:
            case PGRES_COPY_IN:
            case PGRES_COPY_BOTH:
            case PGRES_SINGLE_TUPLE:
            case PGRES_PIPELINE_SYNC:
                return std::nullopt;  // Success

            // TODO: Pipeline mode is not yet implemented. When the Pipeline class is added,
            // PGRES_PIPELINE_ABORTED should be mapped to a dedicated PipelineAborted error code
            // and handled in the pipeline execution path. For now, treat as a generic error.
            case PGRES_PIPELINE_ABORTED:
                return ErrorCode(ServerErrorCode::TransactionAborted);

            case PGRES_BAD_RESPONSE:
                return ErrorCode(FatalErrorCode::ProtocolViolation);

            case PGRES_NONFATAL_ERROR:
            case PGRES_FATAL_ERROR:
                // Caller should parse SQLSTATE for specific error classification
                return ErrorCode(ServerErrorCode::RuntimeError);
        }

        return ErrorCode(FatalErrorCode::UnexpectedState);
    }

}  // namespace demiplane::db::postgres::detail
