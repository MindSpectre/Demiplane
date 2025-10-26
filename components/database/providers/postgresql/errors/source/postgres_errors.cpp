#include "postgres_errors.hpp"

#include <string>

namespace demiplane::db::postgres {

    // ============== SQLSTATE Mapping Implementation ==============

    /**
     * @brief Map PostgreSQL SQLSTATE code to ErrorCode
     *
     * PostgreSQL uses 5-character SQLSTATE codes (SQL standard).
     * This function maps them to our unified error code system.
     *
     * @param sqlstate 5-character SQLSTATE string (e.g., "23505")
     * @return ErrorCode if error, nullopt if success (00000)
     *
     * @see https://www.postgresql.org/docs/current/errcodes-appendix.html
     *
     */
    std::optional<ErrorCode> map_sqlstate(const std::string_view sqlstate) noexcept {
        if (sqlstate.empty() || sqlstate == "00000") {
            return std::nullopt;  // Success
        }

        // Get the class (first 2 characters)
        const std::string_view error_class = sqlstate.substr(0, 2);

        // -------- Class 00: Successful Completion --------
        if (error_class == "00") {
            return std::nullopt;
        }

        // -------- Class 08: Connection Exception --------
        if (error_class == "08") {
            if (sqlstate == "08000")
                return ErrorCode(ServerErrorCode::ConnectionError);
            if (sqlstate == "08003")
                return ErrorCode(ClientErrorCode::NotConnected);
            if (sqlstate == "08006")
                return ErrorCode(ServerErrorCode::ConnectionLost);
            if (sqlstate == "08P01")
                return ErrorCode(FatalErrorCode::ProtocolViolation);
            return ErrorCode(ServerErrorCode::ConnectionError);
        }

        // -------- Class 0A: Feature Not Supported --------
        if (error_class == "0A") {
            return ErrorCode(ClientErrorCode::InvalidOption);
        }

        // -------- Class 20: Case Not Found --------
        if (error_class == "20") {
            return ErrorCode(ServerErrorCode::ObjectNotFound);
        }

        // -------- Class 21: Cardinality Violation --------
        if (error_class == "21") {
            return ErrorCode(ServerErrorCode::DataError);
        }

        // -------- Class 22: Data Exception --------
        if (error_class == "22") {
            if (sqlstate == "22000")
                return ErrorCode(ServerErrorCode::DataError);
            if (sqlstate == "22001")
                return ErrorCode(ServerErrorCode::DataTooLong);
            if (sqlstate == "22003")
                return ErrorCode(ServerErrorCode::NumericOverflow);
            if (sqlstate == "22007")
                return ErrorCode(ServerErrorCode::InvalidDatetime);
            if (sqlstate == "22008")
                return ErrorCode(ServerErrorCode::InvalidDatetime);
            if (sqlstate == "22012")
                return ErrorCode(ServerErrorCode::DivisionByZero);
            if (sqlstate == "22P02")
                return ErrorCode(ServerErrorCode::InvalidTextFormat);
            if (sqlstate == "22P03")
                return ErrorCode(ServerErrorCode::InvalidEncoding);
            if (sqlstate == "22P04")
                return ErrorCode(ServerErrorCode::InvalidTextFormat);
            return ErrorCode(ServerErrorCode::DataError);
        }

        // -------- Class 23: Integrity Constraint Violation --------
        if (error_class == "23") {
            if (sqlstate == "23000")
                return ErrorCode(ServerErrorCode::ConstraintViolation);
            if (sqlstate == "23001")
                return ErrorCode(ServerErrorCode::ConstraintViolation);
            if (sqlstate == "23502")
                return ErrorCode(ServerErrorCode::NotNullViolation);
            if (sqlstate == "23503")
                return ErrorCode(ServerErrorCode::ForeignKeyViolation);
            if (sqlstate == "23505")
                return ErrorCode(ServerErrorCode::UniqueViolation);
            if (sqlstate == "23514")
                return ErrorCode(ServerErrorCode::CheckViolation);
            if (sqlstate == "23P01")
                return ErrorCode(ServerErrorCode::ExclusionViolation);
            return ErrorCode(ServerErrorCode::ConstraintViolation);
        }

        // -------- Class 24: Invalid Cursor State --------
        if (error_class == "24") {
            return ErrorCode(ClientErrorCode::InvalidState);
        }

        // -------- Class 25: Invalid Transaction State --------
        if (error_class == "25") {
            if (sqlstate == "25000")
                return ErrorCode(ClientErrorCode::InvalidState);
            if (sqlstate == "25001")
                return ErrorCode(ClientErrorCode::TransactionActive);
            if (sqlstate == "25002")
                return ErrorCode(ClientErrorCode::InvalidState);
            if (sqlstate == "25003")
                return ErrorCode(ClientErrorCode::InvalidState);
            if (sqlstate == "25004")
                return ErrorCode(ClientErrorCode::InvalidState);
            if (sqlstate == "25005")
                return ErrorCode(ClientErrorCode::InvalidState);
            if (sqlstate == "25006")
                return ErrorCode(ClientErrorCode::InvalidState);
            if (sqlstate == "25007")
                return ErrorCode(ClientErrorCode::InvalidState);
            if (sqlstate == "25008")
                return ErrorCode(ClientErrorCode::InvalidState);
            if (sqlstate == "25P01")
                return ErrorCode(ClientErrorCode::NoActiveTransaction);
            if (sqlstate == "25P02")
                return ErrorCode(ClientErrorCode::TransactionActive);
            if (sqlstate == "25P03")
                return ErrorCode(ClientErrorCode::NoActiveTransaction);
            return ErrorCode(ClientErrorCode::InvalidState);
        }

        // -------- Class 26: Invalid SQL Statement Name --------
        if (error_class == "26") {
            return ErrorCode(ClientErrorCode::InvalidArgument);
        }

        // -------- Class 28: Invalid Authorization Specification --------
        if (error_class == "28") {
            if (sqlstate == "28000")
                return ErrorCode(ClientErrorCode::AuthenticationError);
            if (sqlstate == "28P01")
                return ErrorCode(ClientErrorCode::AuthenticationError);
            return ErrorCode(ClientErrorCode::AuthenticationError);
        }

        // -------- Class 2B: Dependent Privilege Descriptors Still Exist --------
        if (error_class == "2B") {
            return ErrorCode(ServerErrorCode::ConstraintViolation);
        }

        // -------- Class 2D: Invalid Transaction Termination --------
        if (error_class == "2D") {
            return ErrorCode(ServerErrorCode::TransactionError);
        }

        // -------- Class 2F: SQL Routine Exception --------
        if (error_class == "2F") {
            return ErrorCode(ServerErrorCode::RuntimeError);
        }

        // -------- Class 34: Invalid Cursor Name --------
        if (error_class == "34") {
            return ErrorCode(ClientErrorCode::InvalidArgument);
        }

        // -------- Class 38: External Routine Exception --------
        if (error_class == "38") {
            return ErrorCode(ServerErrorCode::RuntimeError);
        }

        // -------- Class 39: External Routine Invocation Exception --------
        if (error_class == "39") {
            return ErrorCode(ServerErrorCode::RuntimeError);
        }

        // -------- Class 3B: Savepoint Exception --------
        if (error_class == "3B") {
            return ErrorCode(ServerErrorCode::TransactionError);
        }

        // -------- Class 3D: Invalid Catalog Name --------
        if (error_class == "3D") {
            return ErrorCode(ServerErrorCode::DatabaseNotFound);
        }

        // -------- Class 3F: Invalid Schema Name --------
        if (error_class == "3F") {
            return ErrorCode(ServerErrorCode::SchemaNotFound);
        }

        // -------- Class 40: Transaction Rollback --------
        if (error_class == "40") {
            if (sqlstate == "40000")
                return ErrorCode(ServerErrorCode::TransactionRollback);
            if (sqlstate == "40001")
                return ErrorCode(ServerErrorCode::SerializationFailure);
            if (sqlstate == "40002")
                return ErrorCode(ServerErrorCode::TransactionAborted);
            if (sqlstate == "40003")
                return ErrorCode(ServerErrorCode::TransactionAborted);
            if (sqlstate == "40P01")
                return ErrorCode(ServerErrorCode::DeadlockDetected);
            return ErrorCode(ServerErrorCode::TransactionRollback);
        }

        // -------- Class 42: Syntax Error or Access Rule Violation --------
        if (error_class == "42") {
            if (sqlstate == "42000")
                return ErrorCode(ClientErrorCode::SyntaxError);
            if (sqlstate == "42501")
                return ErrorCode(ServerErrorCode::PermissionDenied);
            if (sqlstate == "42601")
                return ErrorCode(ClientErrorCode::SyntaxError);
            if (sqlstate == "42602")
                return ErrorCode(ClientErrorCode::InvalidArgument);
            if (sqlstate == "42611")
                return ErrorCode(ClientErrorCode::InvalidArgument);
            if (sqlstate == "42622")
                return ErrorCode(ClientErrorCode::InvalidArgument);
            if (sqlstate == "42701")
                return ErrorCode(ClientErrorCode::InvalidArgument);
            if (sqlstate == "42702")
                return ErrorCode(ClientErrorCode::InvalidArgument);
            if (sqlstate == "42703")
                return ErrorCode(ServerErrorCode::ColumnNotFound);
            if (sqlstate == "42704")
                return ErrorCode(ServerErrorCode::ObjectNotFound);
            if (sqlstate == "42710")
                return ErrorCode(ClientErrorCode::InvalidArgument);
            if (sqlstate == "42712")
                return ErrorCode(ClientErrorCode::InvalidArgument);
            if (sqlstate == "42723")
                return ErrorCode(ClientErrorCode::InvalidArgument);
            if (sqlstate == "42725")
                return ErrorCode(ClientErrorCode::InvalidArgument);
            if (sqlstate == "42803")
                return ErrorCode(ClientErrorCode::InvalidArgument);
            if (sqlstate == "42804")
                return ErrorCode(ClientErrorCode::TypeMismatch);
            if (sqlstate == "42809")
                return ErrorCode(ClientErrorCode::InvalidArgument);
            if (sqlstate == "42830")
                return ErrorCode(ServerErrorCode::PermissionDenied);
            if (sqlstate == "42846")
                return ErrorCode(ClientErrorCode::TypeMismatch);
            if (sqlstate == "42883")
                return ErrorCode(ServerErrorCode::FunctionNotFound);
            if (sqlstate == "42939")
                return ErrorCode(ClientErrorCode::InvalidArgument);
            if (sqlstate == "42P01")
                return ErrorCode(ServerErrorCode::TableNotFound);
            if (sqlstate == "42P02")
                return ErrorCode(ClientErrorCode::InvalidParameter);
            if (sqlstate == "42P03")
                return ErrorCode(ClientErrorCode::InvalidArgument);
            if (sqlstate == "42P04")
                return ErrorCode(ServerErrorCode::DatabaseNotFound);
            if (sqlstate == "42P05")
                return ErrorCode(ClientErrorCode::InvalidArgument);
            if (sqlstate == "42P06")
                return ErrorCode(ServerErrorCode::SchemaNotFound);
            if (sqlstate == "42P07")
                return ErrorCode(ClientErrorCode::InvalidArgument);
            if (sqlstate == "42P08")
                return ErrorCode(ClientErrorCode::InvalidArgument);
            if (sqlstate == "42P09")
                return ErrorCode(ClientErrorCode::InvalidArgument);
            if (sqlstate == "42P10")
                return ErrorCode(ClientErrorCode::InvalidArgument);
            if (sqlstate == "42P11")
                return ErrorCode(ClientErrorCode::InvalidArgument);
            if (sqlstate == "42P12")
                return ErrorCode(ClientErrorCode::InvalidArgument);
            if (sqlstate == "42P13")
                return ErrorCode(ClientErrorCode::InvalidArgument);
            if (sqlstate == "42P14")
                return ErrorCode(ClientErrorCode::InvalidArgument);
            if (sqlstate == "42P15")
                return ErrorCode(ServerErrorCode::SchemaNotFound);
            if (sqlstate == "42P16")
                return ErrorCode(ClientErrorCode::InvalidArgument);
            if (sqlstate == "42P17")
                return ErrorCode(ClientErrorCode::InvalidArgument);
            if (sqlstate == "42P18")
                return ErrorCode(ClientErrorCode::TypeMismatch);
            if (sqlstate == "42P19")
                return ErrorCode(ClientErrorCode::InvalidArgument);
            if (sqlstate == "42P20")
                return ErrorCode(ClientErrorCode::InvalidArgument);
            if (sqlstate == "42P21")
                return ErrorCode(ClientErrorCode::InvalidArgument);
            if (sqlstate == "42P22")
                return ErrorCode(ClientErrorCode::InvalidArgument);
            return ErrorCode(ClientErrorCode::SyntaxError);
        }

        // -------- Class 44: WITH CHECK OPTION Violation --------
        if (error_class == "44") {
            return ErrorCode(ServerErrorCode::CheckViolation);
        }

        // -------- Class 53: Insufficient Resources --------
        if (error_class == "53") {
            if (sqlstate == "53000")
                return ErrorCode(ServerErrorCode::ResourceError);
            if (sqlstate == "53100")
                return ErrorCode(ServerErrorCode::DiskFull);
            if (sqlstate == "53200")
                return ErrorCode(ServerErrorCode::OutOfMemory);
            if (sqlstate == "53300")
                return ErrorCode(ServerErrorCode::TooManyConnections);
            if (sqlstate == "53400")
                return ErrorCode(ServerErrorCode::ConfigurationLimit);
            return ErrorCode(ServerErrorCode::ResourceError);
        }

        // -------- Class 54: Program Limit Exceeded --------
        if (error_class == "54") {
            if (sqlstate == "54000")
                return ErrorCode(ServerErrorCode::ConfigurationLimit);
            if (sqlstate == "54001")
                return ErrorCode(ServerErrorCode::QueryTooComplex);
            if (sqlstate == "54011")
                return ErrorCode(ServerErrorCode::TooManyConnections);
            if (sqlstate == "54023")
                return ErrorCode(ServerErrorCode::TooManyConnections);
            return ErrorCode(ServerErrorCode::ConfigurationLimit);
        }

        // -------- Class 55: Object Not In Prerequisite State --------
        if (error_class == "55") {
            if (sqlstate == "55000")
                return ErrorCode(ClientErrorCode::InvalidState);
            if (sqlstate == "55006")
                return ErrorCode(ClientErrorCode::InvalidState);
            if (sqlstate == "55P02")
                return ErrorCode(ServerErrorCode::LockTimeout);
            if (sqlstate == "55P03")
                return ErrorCode(ServerErrorCode::LockTimeout);
            return ErrorCode(ClientErrorCode::InvalidState);
        }

        // -------- Class 57: Operator Intervention --------
        if (error_class == "57") {
            if (sqlstate == "57000")
                return ErrorCode(ServerErrorCode::RuntimeError);
            if (sqlstate == "57014")
                return ErrorCode(ServerErrorCode::StatementTimeout);
            if (sqlstate == "57P01")
                return ErrorCode(ServerErrorCode::ConnectionError);
            if (sqlstate == "57P02")
                return ErrorCode(ServerErrorCode::ConnectionError);
            if (sqlstate == "57P03")
                return ErrorCode(ServerErrorCode::ConnectionError);
            if (sqlstate == "57P04")
                return ErrorCode(ServerErrorCode::ConnectionError);
            if (sqlstate == "57P05")
                return ErrorCode(ServerErrorCode::ConnectionError);
            return ErrorCode(ServerErrorCode::RuntimeError);
        }

        // -------- Class 58: System Error --------
        if (error_class == "58") {
            if (sqlstate == "58000")
                return ErrorCode(FatalErrorCode::InternalError);
            if (sqlstate == "58030")
                return ErrorCode(FatalErrorCode::CorruptionDetected);
            if (sqlstate == "58P01")
                return ErrorCode(FatalErrorCode::InternalError);
            if (sqlstate == "58P02")
                return ErrorCode(FatalErrorCode::InternalError);
            return ErrorCode(FatalErrorCode::InternalError);
        }

        // -------- Class F0: Configuration File Error --------
        if (error_class == "F0") {
            return ErrorCode(ClientErrorCode::ConfigurationError);
        }

        // -------- Class HV: Foreign Data Wrapper Error --------
        if (error_class == "HV") {
            return ErrorCode(ServerErrorCode::RuntimeError);
        }

        // -------- Class P0: PL/pgSQL Error --------
        if (error_class == "P0") {
            if (sqlstate == "P0000")
                return ErrorCode(ServerErrorCode::RuntimeError);
            if (sqlstate == "P0001")
                return ErrorCode(ServerErrorCode::RuntimeError);
            if (sqlstate == "P0002")
                return ErrorCode(ServerErrorCode::ObjectNotFound);
            if (sqlstate == "P0003")
                return ErrorCode(ServerErrorCode::DataError);
            if (sqlstate == "P0004")
                return ErrorCode(ClientErrorCode::InvalidParameter);
            return ErrorCode(ServerErrorCode::RuntimeError);
        }

        // -------- Class XX: Internal Error --------
        if (error_class == "XX") {
            if (sqlstate == "XX000")
                return ErrorCode(FatalErrorCode::InternalError);
            if (sqlstate == "XX001")
                return ErrorCode(FatalErrorCode::CorruptionDetected);
            if (sqlstate == "XX002")
                return ErrorCode(FatalErrorCode::CorruptionDetected);
            return ErrorCode(FatalErrorCode::InternalError);
        }

        // Unknown error class - treat as fatal
        return ErrorCode(FatalErrorCode::UnexpectedState);
    }

    // ============== ExecStatusType Mapping Implementation ==============
    /**
     * @brief Map PostgreSQL ExecStatusType to ErrorCode
     *
     * For quick error checking without detailed SQLSTATE parsing.
     *
     * @param status Result status from PQresultStatus
     * @return ErrorCode if error, nullopt if success
     *
     */
    std::optional<ErrorCode> map_exec_status(const ExecStatusType status) noexcept {
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
            case PGRES_PIPELINE_ABORTED:
                return std::nullopt;  // Success

            case PGRES_BAD_RESPONSE:
                return ErrorCode(FatalErrorCode::ProtocolViolation);

            case PGRES_NONFATAL_ERROR:
                return ErrorCode(ServerErrorCode::RuntimeError);

            case PGRES_FATAL_ERROR:
                // Should parse SQLSTATE for specific error (caller's responsibility)
                return ErrorCode(ServerErrorCode::RuntimeError);
        }

        return ErrorCode(FatalErrorCode::UnexpectedState);
    }

    // ============== Error Extraction Implementation ==============

    std::optional<ErrorContext> extract_error(const PGresult* result) {
        if (!result) {
            return ErrorContext(ErrorCode(FatalErrorCode::UnexpectedState));
        }

        const ExecStatusType status = PQresultStatus(result);

        // Check if it's actually an error
        if (status == PGRES_COMMAND_OK || status == PGRES_TUPLES_OK) {
            return std::nullopt;  // Success
        }

        ErrorContext ctx;

        // Get SQLSTATE (most important for accurate mapping)
        if (const char* sqlstate = PQresultErrorField(result, PG_DIAG_SQLSTATE)) {
            ctx.sqlstate = sqlstate;
            if (const auto code = map_sqlstate(sqlstate)) {
                ctx.code = *code;
            } else {
                return std::nullopt;
            }
        } else {
            // Fallback to status-based mapping
            if (const auto code = map_exec_status(status)) {
                ctx.code = *code;
            } else {
                return std::nullopt;
            }
        }

        // Get primary message
        if (const char* msg = PQresultErrorMessage(result)) {
            ctx.message = msg;
        }

        // Get detail
        if (const char* detail = PQresultErrorField(result, PG_DIAG_MESSAGE_DETAIL)) {
            ctx.detail = detail;
        }

        // Get hint
        if (const char* hint = PQresultErrorField(result, PG_DIAG_MESSAGE_HINT)) {
            ctx.hint = hint;
        }

        // Get context
        if (const char* context = PQresultErrorField(result, PG_DIAG_CONTEXT)) {
            ctx.context = context;
        }

        // Get statement position
        if (const char* pos = PQresultErrorField(result, PG_DIAG_STATEMENT_POSITION)) {
            try {
                ctx.position = std::stoi(pos);
            } catch (...) {
                // Ignore parse errors
            }
        }

        return ctx;
    }

    std::optional<ErrorCode> extract_error_code(const PGresult* result) {
        if (!result) {
            return ErrorCode(FatalErrorCode::UnexpectedState);
        }

        const ExecStatusType status = PQresultStatus(result);

        if (status == PGRES_COMMAND_OK || status == PGRES_TUPLES_OK) {
            return std::nullopt;  // Success
        }

        // Try SQLSTATE first for accuracy
        if (const char* sqlstate = PQresultErrorField(result, PG_DIAG_SQLSTATE)) {
            return map_sqlstate(sqlstate);
        }

        // Fallback to status
        return map_exec_status(status);
    }

}  // namespace demiplane::db::postgres
