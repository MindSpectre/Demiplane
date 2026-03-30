#pragma once

// ============== SQLSTATE → ErrorCode Mapping ==============
//
// Maps PostgreSQL 5-character SQLSTATE codes to our unified ErrorCode system.
// Dispatches by 2-char class prefix, then refines by specific code.
//
// Reference: https://www.postgresql.org/docs/current/errcodes-appendix.html
//
// Included by postgres_errors.cpp — not a standalone translation unit.

#include <optional>
#include <string_view>

#include <db_error_codes.hpp>

namespace demiplane::db::postgres::detail {

    [[nodiscard]] inline std::optional<ErrorCode> map_sqlstate(const std::string_view sqlstate) noexcept {
        if (sqlstate.empty() || sqlstate == "00000") {
            return std::nullopt;  // Success
        }

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
            if (sqlstate == "25001")
                return ErrorCode(ClientErrorCode::TransactionActive);
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
            if (sqlstate == "42501")
                return ErrorCode(ServerErrorCode::PermissionDenied);
            if (sqlstate == "42601")
                return ErrorCode(ClientErrorCode::SyntaxError);
            if (sqlstate == "42703")
                return ErrorCode(ServerErrorCode::ColumnNotFound);
            if (sqlstate == "42704")
                return ErrorCode(ServerErrorCode::ObjectNotFound);
            if (sqlstate == "42804" || sqlstate == "42846" || sqlstate == "42P18")
                return ErrorCode(ClientErrorCode::TypeMismatch);
            if (sqlstate == "42830")
                return ErrorCode(ServerErrorCode::PermissionDenied);
            if (sqlstate == "42883")
                return ErrorCode(ServerErrorCode::FunctionNotFound);
            if (sqlstate == "42P01")
                return ErrorCode(ServerErrorCode::TableNotFound);
            if (sqlstate == "42P02")
                return ErrorCode(ClientErrorCode::InvalidParameter);
            if (sqlstate == "42P04")
                return ErrorCode(ServerErrorCode::DatabaseNotFound);
            if (sqlstate == "42P06" || sqlstate == "42P15")
                return ErrorCode(ServerErrorCode::SchemaNotFound);
            // All other 42xxx codes are syntax/argument errors
            return ErrorCode(ClientErrorCode::SyntaxError);
        }

        // -------- Class 44: WITH CHECK OPTION Violation --------
        if (error_class == "44") {
            return ErrorCode(ServerErrorCode::CheckViolation);
        }

        // -------- Class 53: Insufficient Resources --------
        if (error_class == "53") {
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
            if (sqlstate == "54001")
                return ErrorCode(ServerErrorCode::QueryTooComplex);
            if (sqlstate == "54011" || sqlstate == "54023")
                return ErrorCode(ServerErrorCode::TooManyConnections);
            return ErrorCode(ServerErrorCode::ConfigurationLimit);
        }

        // -------- Class 55: Object Not In Prerequisite State --------
        if (error_class == "55") {
            if (sqlstate == "55P02" || sqlstate == "55P03")
                return ErrorCode(ServerErrorCode::LockTimeout);
            return ErrorCode(ClientErrorCode::InvalidState);
        }

        // -------- Class 57: Operator Intervention --------
        if (error_class == "57") {
            if (sqlstate == "57014")
                return ErrorCode(ServerErrorCode::StatementTimeout);
            if (sqlstate.starts_with("57P"))
                return ErrorCode(ServerErrorCode::ConnectionError);
            return ErrorCode(ServerErrorCode::RuntimeError);
        }

        // -------- Class 58: System Error --------
        if (error_class == "58") {
            if (sqlstate == "58030")
                return ErrorCode(FatalErrorCode::CorruptionDetected);
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
            if (sqlstate == "XX001" || sqlstate == "XX002")
                return ErrorCode(FatalErrorCode::CorruptionDetected);
            return ErrorCode(FatalErrorCode::InternalError);
        }

        // Unknown error class
        return ErrorCode(FatalErrorCode::UnexpectedState);
    }

}  // namespace demiplane::db::postgres::detail
