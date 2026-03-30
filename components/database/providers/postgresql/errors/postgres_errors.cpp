#include "postgres_errors.hpp"

#include <charconv>
#include <cstring>

#include "detail/exec_status_mapping.inl"
#include "detail/sqlstate_mapping.inl"

namespace demiplane::db::postgres {

    // ============== Error Extraction ==============
    //
    // Call graph:
    //
    //   extract_error(PGresult*)          → full ErrorContext (SQLSTATE + all diagnostic fields)
    //     ├─ detail::map_sqlstate()       → ErrorCode from 5-char SQLSTATE (preferred)
    //     └─ detail::map_exec_status()    → ErrorCode from ExecStatusType (fallback)
    //
    //   extract_error_code(PGresult*)     → ErrorCode only (lightweight, no string allocation)
    //     ├─ detail::map_sqlstate()
    //     └─ detail::map_exec_status()
    //
    //   extract_connection_error(PGconn*) → ErrorContext from connection state (inline, header-only)
    //   check_connection(PGconn*)         → ErrorCode from connection status   (inline, header-only)
    //

    // ---------- Success status check ----------

    [[nodiscard]] static bool is_success_status(const ExecStatusType status) noexcept {
        switch (status) {
            case PGRES_COMMAND_OK:
            case PGRES_TUPLES_OK:
            case PGRES_COPY_OUT:
            case PGRES_COPY_IN:
            case PGRES_COPY_BOTH:
            case PGRES_SINGLE_TUPLE:
            case PGRES_PIPELINE_SYNC:
                return true;
            default:
                return false;
        }
    }

    // ---------- Full error extraction ----------

    std::optional<ErrorContext> extract_error(const PGresult* result) {
        if (!result) {
            return ErrorContext(ErrorCode(FatalErrorCode::UnexpectedState));
        }

        const ExecStatusType status = PQresultStatus(result);

        if (is_success_status(status)) {
            return std::nullopt;
        }

        ErrorContext ctx;

        // SQLSTATE is most accurate — try it first
        if (const char* sqlstate = PQresultErrorField(result, PG_DIAG_SQLSTATE)) {
            ctx.sqlstate = sqlstate;
            if (const auto code = detail::map_sqlstate(sqlstate)) {
                ctx.code = *code;
            } else {
                return std::nullopt;  // SQLSTATE indicates success (00xxx)
            }
        } else {
            // No SQLSTATE available — fall back to ExecStatusType
            if (const auto code = detail::map_exec_status(status)) {
                ctx.code = *code;
            } else {
                return std::nullopt;  // Status indicates success
            }
        }

        // Collect diagnostic fields
        if (const char* msg = PQresultErrorMessage(result)) {
            ctx.message = msg;
        }

        if (const char* detail = PQresultErrorField(result, PG_DIAG_MESSAGE_DETAIL)) {
            ctx.detail = detail;
        }

        if (const char* hint = PQresultErrorField(result, PG_DIAG_MESSAGE_HINT)) {
            ctx.hint = hint;
        }

        if (const char* context = PQresultErrorField(result, PG_DIAG_CONTEXT)) {
            ctx.context = context;
        }

        if (const char* pos = PQresultErrorField(result, PG_DIAG_STATEMENT_POSITION)) {
            int value = 0;
            if (auto [ptr, ec] = std::from_chars(pos, pos + std::strlen(pos), value); ec == std::errc{}) {
                ctx.position = value;
            }
        }

        return ctx;
    }

    // ---------- Lightweight error code extraction ----------

    std::optional<ErrorCode> extract_error_code(const PGresult* result) {
        if (!result) {
            return ErrorCode(FatalErrorCode::UnexpectedState);
        }

        const ExecStatusType status = PQresultStatus(result);

        if (is_success_status(status)) {
            return std::nullopt;
        }

        // SQLSTATE first for accuracy
        if (const char* sqlstate = PQresultErrorField(result, PG_DIAG_SQLSTATE)) {
            return detail::map_sqlstate(sqlstate);
        }

        // Fall back to status
        return detail::map_exec_status(status);
    }

}  // namespace demiplane::db::postgres
