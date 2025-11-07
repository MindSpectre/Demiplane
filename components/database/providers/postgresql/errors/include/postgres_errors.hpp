#pragma once

#include <optional>
#include <ostream>
#include <string>
#include <string_view>

#include <db_error_codes.hpp>
#include <libpq-fe.h>

namespace demiplane::db::postgres {

    // ============== Error Context ==============

    /**
     * @brief Rich error context from PostgreSQL
     *
     * Captures all available error information from PGresult
     * for detailed error reporting and logging.
     */
    struct ErrorContext {
        ErrorCode code;
        std::string sqlstate;         // 5-character SQLSTATE code
        std::string message;          // Primary error message
        std::string detail;           // Detailed error message
        std::string hint;             // Hint for fixing the error
        std::string context;          // Error context (line number, etc.)
        std::optional<int> position;  // Character position in query

        ErrorContext() = default;

        explicit ErrorContext(const ErrorCode ec)
            : code(ec) {
        }

        [[nodiscard]] bool has_error() const noexcept {
            return !code.is_success();
        }

        [[nodiscard]] std::string format() const {
            if (!has_error()) {
                return "Success";
            }

            std::string result = "[" + std::string(code.name()) + "] ";

            if (!sqlstate.empty()) {
                result += "SQLSTATE " + sqlstate + ": ";
            }

            result += message;

            if (!detail.empty()) {
                result += "\nDetail: " + detail;
            }

            if (!hint.empty()) {
                result += "\nHint: " + hint;
            }

            if (!context.empty()) {
                result += "\nContext: " + context;
            }

            if (position.has_value()) {
                result += "\nPosition: " + std::to_string(*position);
            }

            return result;
        }
    };

    // ============== SQLSTATE Mapping (Compiled) ==============


    // ============== Error Extraction (Compiled) ==============

    /**
     * @brief Extract comprehensive error information from PGresult
     *
     * Parses SQLSTATE, error messages, and all diagnostic fields
     * to create a rich ErrorContext.
     *
     * @param result PostgreSQL result object (can be nullptr)
     * @return ErrorContext if error, nullopt if success
     *
     * @note Compiled separately to keep header lightweight.
     */
    std::optional<ErrorContext> extract_error(const PGresult* result);

    /**
     * @brief Extract error code only from PGresult (lightweight)
     *
     * Use when you only need the error code, not full context.
     *
     * @param result PostgreSQL result object
     * @return ErrorCode if error, nullopt if success
     *
     * @note Compiled separately to keep header lightweight.
     */
    std::optional<ErrorCode> extract_error_code(const PGresult* result);

    // ============== Stream Operators (Inline) ==============

    /**
     * @brief Stream output operator for ErrorContext
     *
     * Enables easy debugging output for ErrorContext objects.
     * Example: std::cout << error_ctx << std::endl;
     *
     * @param os Output stream
     * @param ctx ErrorContext to output
     * @return Reference to the output stream
     */
    inline std::ostream& operator<<(std::ostream& os, const ErrorContext& ctx) {
        return os << ctx.format();
    }

    // ============== Connection Error Helpers (Inline) ==============

    /**
     * @brief Extract error from connection object
     *
     * Used when connection operations fail (e.g., PQsendQuery returns 0).
     *
     * @param conn PostgreSQL connection object
     * @return ErrorContext with connection error
     *
     * @note Kept inline - small function, error path only.
     */
    inline ErrorContext extract_connection_error(const PGconn* conn) {
        if (!conn) {
            return ErrorContext(ErrorCode(ClientErrorCode::NotConnected));
        }

        ErrorContext ctx;

        // Check connection status
        if (const ConnStatusType status = PQstatus(conn); status == CONNECTION_BAD) {
            ctx.code = ErrorCode(ServerErrorCode::ConnectionLost);
        } else {
            // Some other error occurred
            ctx.code = ErrorCode(ServerErrorCode::RuntimeError);
        }

        // Get error message
        if (const char* msg = PQerrorMessage(conn); msg && msg[0] != '\0') {
            ctx.message = msg;
        } else {
            ctx.message = "Unknown connection error";
        }

        return ctx;
    }

    /**
     * @brief Check if connection is healthy
     *
     * @param conn PostgreSQL connection
     * @return ErrorCode (Success if healthy, error otherwise)
     *
     * @note Kept inline - small function, simple switch statement.
     */
    inline ErrorCode check_connection(const PGconn* conn) noexcept {
        if (!conn) {
            return ErrorCode(ClientErrorCode::NotConnected);
        }

        switch (PQstatus(conn)) {
            case CONNECTION_OK:
                return ErrorCode{};
            case CONNECTION_BAD:
                return ErrorCode(ServerErrorCode::ConnectionLost);
            case CONNECTION_STARTED:
            case CONNECTION_MADE:
            case CONNECTION_AWAITING_RESPONSE:
            case CONNECTION_AUTH_OK:
            case CONNECTION_SETENV:
            case CONNECTION_SSL_STARTUP:
            case CONNECTION_NEEDED:
            case CONNECTION_CHECK_WRITABLE:
            case CONNECTION_CONSUME:
            case CONNECTION_GSS_STARTUP:
            case CONNECTION_CHECK_TARGET:
            case CONNECTION_CHECK_STANDBY:
                return ErrorCode(ClientErrorCode::InvalidState);
        }

        return ErrorCode(FatalErrorCode::UnexpectedState);
    }

}  // namespace demiplane::db::postgres
