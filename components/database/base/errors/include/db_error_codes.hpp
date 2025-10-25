#pragma once
#include <cstdint>
#include <optional>
#include <utility>

namespace demiplane::db {

    // ============== Category-Specific Error Enums ==============

    /**
     * @brief Client-side error codes
     *
     * Errors that originate from incorrect usage, invalid input,
     * or configuration issues on the client side.
     */
    enum class ClientErrorCode : uint16_t {
        Success = 0,

        // Invalid Arguments (100-199)
        InvalidArgument  = 100,
        SyntaxError      = 101,
        InvalidParameter = 102,
        TypeMismatch     = 103,
        NullConversion   = 104,
        InvalidCast      = 105,
        OutOfRange       = 106,

        // Configuration Errors (200-299)
        ConfigurationError    = 200,
        ConnectionStringError = 201,
        AuthenticationError   = 202,
        InvalidOption         = 203,
        MissingParameter      = 204,

        // State Errors (300-399)
        InvalidState        = 300,
        NotConnected        = 301,
        AlreadyConnected    = 302,
        TransactionActive   = 303,
        NoActiveTransaction = 304,
    };

    /**
     * @brief Server-side error codes
     *
     * Errors that originate from the database server, including
     * constraint violations, resource issues, and runtime errors.
     */
    enum class ServerErrorCode : uint16_t {
        Success = 0,

        // Runtime Errors (100-199) - Generally retryable
        RuntimeError         = 100,
        ConnectionError      = 110,
        ConnectionLost       = 111,
        ConnectionTimeout    = 112,
        ConnectionRefused    = 113,
        DeadlockDetected     = 120,
        LockTimeout          = 121,
        StatementTimeout     = 122,
        SerializationFailure = 123,

        // Constraint Violations (200-299) - Not retryable
        ConstraintViolation = 200,
        UniqueViolation     = 201,
        ForeignKeyViolation = 202,
        CheckViolation      = 203,
        NotNullViolation    = 204,
        ExclusionViolation  = 205,

        // Data Errors (300-399) - Not retryable
        DataError         = 300,
        DataTooLong       = 301,
        NumericOverflow   = 302,
        DivisionByZero    = 303,
        InvalidDatetime   = 304,
        InvalidEncoding   = 305,
        InvalidTextFormat = 306,

        // Access Errors (400-499) - Not retryable
        AccessError      = 400,
        PermissionDenied = 401,
        ObjectNotFound   = 402,
        DatabaseNotFound = 403,
        TableNotFound    = 404,
        ColumnNotFound   = 405,
        SchemaNotFound   = 406,
        FunctionNotFound = 407,

        // Resource Errors (500-599) - Generally retryable
        ResourceError      = 500,
        OutOfMemory        = 501,
        DiskFull           = 502,
        TooManyConnections = 503,
        ConfigurationLimit = 504,
        QueryTooComplex    = 505,

        // Transaction Errors (600-699) - May be retryable
        TransactionError      = 600,
        TransactionRollback   = 601,
        TransactionAborted    = 602,
        InvalidIsolationLevel = 603,
    };

    /**
     * @brief Fatal error codes
     *
     * Errors that indicate critical failures, corruption, or
     * unrecoverable states. These are never retryable.
     */
    enum class FatalErrorCode : uint16_t {
        InternalError      = 1,
        CorruptionDetected = 2,
        ProtocolViolation  = 3,
        AssertionFailure   = 4,
        UnexpectedState    = 5,
    };

    // ============== Unified Error Code Class ==============

    /**
     * @brief Type-safe error code wrapper
     *
     * Provides a unified interface for all error code categories
     * while maintaining type safety and category information.
     */
    class ErrorCode {
    public:
        /**
         * @brief Error category enumeration
         */
        enum class Category : uint8_t {
            Success = 0,
            Client  = 1,
            Server  = 2,
            Fatal   = 3,
        };

        // -------- Constructors --------

        /**
         * @brief Default constructor - represents success
         */
        constexpr ErrorCode() noexcept
            : category_(Category::Success),
              code_(0) {
        }

        /**
         * @brief Construct from client error code
         */
        constexpr explicit ErrorCode(ClientErrorCode code) noexcept
            : category_(Category::Client),
              code_(static_cast<uint16_t>(code)) {
        }

        /**
         * @brief Construct from server error code
         */
        constexpr explicit ErrorCode(ServerErrorCode code) noexcept
            : category_(Category::Server),
              code_(static_cast<uint16_t>(code)) {
        }

        /**
         * @brief Construct from fatal error code
         */
        explicit constexpr ErrorCode(FatalErrorCode code) noexcept
            : category_(Category::Fatal),
              code_(static_cast<uint16_t>(code)) {
        }

        // -------- Query Methods --------

        /**
         * @brief Check if this represents success (no error)
         */
        [[nodiscard]] constexpr bool is_success() const noexcept {
            return category_ == Category::Success;
        }

        /**
         * @brief Check if this is a client error
         */
        [[nodiscard]] constexpr bool is_client_error() const noexcept {
            return category_ == Category::Client;
        }

        /**
         * @brief Check if this is a server error
         */
        [[nodiscard]] constexpr bool is_server_error() const noexcept {
            return category_ == Category::Server;
        }

        /**
         * @brief Check if this is a fatal error
         */
        [[nodiscard]] constexpr bool is_fatal_error() const noexcept {
            return category_ == Category::Fatal;
        }

        /**
         * @brief Get the error category
         */
        [[nodiscard]] constexpr Category category() const noexcept {
            return category_;
        }

        /**
         * @brief Get the raw error code value
         */
        [[nodiscard]] constexpr uint16_t value() const noexcept {
            return code_;
        }

        /**
         * @brief Check if this error is retryable
         *
         * Runtime errors (server 100-199), resource errors (server 500-599),
         * and some transaction errors are retryable. Client and fatal errors
         * are never retryable.
         */
        [[nodiscard]] constexpr bool is_retryable() const noexcept {
            if (category_ == Category::Server) {
                // Runtime errors (100-199) are retryable
                if (code_ >= 100 && code_ < 200) {
                    return true;
                }
                // Resource errors (500-599) are retryable
                if (code_ >= 500 && code_ < 600) {
                    return true;
                }
                // Some transaction errors are retryable (600-699)
                if (code_ >= 600 && code_ < 700) {
                    // Deadlocks, serialization failures, aborted transactions
                    return code_ == 601 || code_ == 602;
                }
            }
            return false;
        }

        /**
         * @brief Get human-readable error name
         */
        [[nodiscard]] constexpr const char* name() const noexcept;

        /**
         * @brief Get detailed error description
         */
        [[nodiscard]] constexpr const char* description() const noexcept;

        /**
         * @brief Get category name as string
         */
        [[nodiscard]] constexpr const char* category_name() const noexcept {
            switch (category_) {
                case Category::Success:
                    return "success";
                case Category::Client:
                    return "client";
                case Category::Server:
                    return "server";
                case Category::Fatal:
                    return "fatal";
            }
            std::unreachable();
        }

        // -------- Typed Access --------

        /**
         * @brief Try to get as ClientErrorCode
         * @return Optional containing the code if category matches, empty otherwise
         */
        [[nodiscard]] constexpr std::optional<ClientErrorCode> as_client_error() const noexcept {
            if (category_ == Category::Client) {
                return static_cast<ClientErrorCode>(code_);
            }
            return std::nullopt;
        }

        /**
         * @brief Try to get as ServerErrorCode
         * @return Optional containing the code if category matches, empty otherwise
         */
        [[nodiscard]] constexpr std::optional<ServerErrorCode> as_server_error() const noexcept {
            if (category_ == Category::Server) {
                return static_cast<ServerErrorCode>(code_);
            }
            return std::nullopt;
        }

        /**
         * @brief Try to get as FatalErrorCode
         * @return Optional containing the code if category matches, empty otherwise
         */
        [[nodiscard]] constexpr std::optional<FatalErrorCode> as_fatal_error() const noexcept {
            if (category_ == Category::Fatal) {
                return static_cast<FatalErrorCode>(code_);
            }
            return std::nullopt;
        }

        // -------- Comparison Operators --------

        [[nodiscard]] constexpr bool operator==(const ErrorCode& other) const noexcept {
            return category_ == other.category_ && code_ == other.code_;
        }

        [[nodiscard]] constexpr bool operator!=(const ErrorCode& other) const noexcept {
            return !(*this == other);
        }

        // Allow direct comparison with typed error codes
        [[nodiscard]] constexpr bool operator==(ClientErrorCode other) const noexcept {
            return category_ == Category::Client && code_ == static_cast<uint16_t>(other);
        }

        [[nodiscard]] constexpr bool operator==(ServerErrorCode other) const noexcept {
            return category_ == Category::Server && code_ == static_cast<uint16_t>(other);
        }

        [[nodiscard]] constexpr bool operator==(FatalErrorCode other) const noexcept {
            return category_ == Category::Fatal && code_ == static_cast<uint16_t>(other);
        }

        // -------- Conversion to bool (for convenient checking) --------

        /**
         * @brief Explicit conversion to bool
         * @return true if there is an error (not success), false otherwise
         */
        [[nodiscard]] constexpr explicit operator bool() const noexcept {
            return !is_success();
        }

    private:
        Category category_;
        uint16_t code_;
    };

    // ============== Helper Functions ==============

    /**
     * @brief Get human-readable name for ClientErrorCode
     */

    constexpr const char* to_string(const ClientErrorCode code) noexcept {
        switch (code) {
            case ClientErrorCode::Success:
                return "Success";

                // Invalid Arguments
            case ClientErrorCode::InvalidArgument:
                return "InvalidArgument";
            case ClientErrorCode::SyntaxError:
                return "SyntaxError";
            case ClientErrorCode::InvalidParameter:
                return "InvalidParameter";
            case ClientErrorCode::TypeMismatch:
                return "TypeMismatch";
            case ClientErrorCode::NullConversion:
                return "NullConversion";
            case ClientErrorCode::InvalidCast:
                return "InvalidCast";
            case ClientErrorCode::OutOfRange:
                return "OutOfRange";

                // Configuration Errors
            case ClientErrorCode::ConfigurationError:
                return "ConfigurationError";
            case ClientErrorCode::ConnectionStringError:
                return "ConnectionStringError";
            case ClientErrorCode::AuthenticationError:
                return "AuthenticationError";
            case ClientErrorCode::InvalidOption:
                return "InvalidOption";
            case ClientErrorCode::MissingParameter:
                return "MissingParameter";

                // State Errors
            case ClientErrorCode::InvalidState:
                return "InvalidState";
            case ClientErrorCode::NotConnected:
                return "NotConnected";
            case ClientErrorCode::AlreadyConnected:
                return "AlreadyConnected";
            case ClientErrorCode::TransactionActive:
                return "TransactionActive";
            case ClientErrorCode::NoActiveTransaction:
                return "NoActiveTransaction";
        }
        std::unreachable();
    }

    /**
     * @brief Get human-readable name for ServerErrorCode
     */
    constexpr const char* to_string(const ServerErrorCode code) noexcept {
        switch (code) {
            case ServerErrorCode::Success:
                return "Success";

            // Runtime Errors
            case ServerErrorCode::RuntimeError:
                return "RuntimeError";
            case ServerErrorCode::ConnectionError:
                return "ConnectionError";
            case ServerErrorCode::ConnectionLost:
                return "ConnectionLost";
            case ServerErrorCode::ConnectionTimeout:
                return "ConnectionTimeout";
            case ServerErrorCode::ConnectionRefused:
                return "ConnectionRefused";
            case ServerErrorCode::DeadlockDetected:
                return "DeadlockDetected";
            case ServerErrorCode::LockTimeout:
                return "LockTimeout";
            case ServerErrorCode::StatementTimeout:
                return "StatementTimeout";
            case ServerErrorCode::SerializationFailure:
                return "SerializationFailure";

            // Constraint Violations
            case ServerErrorCode::ConstraintViolation:
                return "ConstraintViolation";
            case ServerErrorCode::UniqueViolation:
                return "UniqueViolation";
            case ServerErrorCode::ForeignKeyViolation:
                return "ForeignKeyViolation";
            case ServerErrorCode::CheckViolation:
                return "CheckViolation";
            case ServerErrorCode::NotNullViolation:
                return "NotNullViolation";
            case ServerErrorCode::ExclusionViolation:
                return "ExclusionViolation";

            // Data Errors
            case ServerErrorCode::DataError:
                return "DataError";
            case ServerErrorCode::DataTooLong:
                return "DataTooLong";
            case ServerErrorCode::NumericOverflow:
                return "NumericOverflow";
            case ServerErrorCode::DivisionByZero:
                return "DivisionByZero";
            case ServerErrorCode::InvalidDatetime:
                return "InvalidDatetime";
            case ServerErrorCode::InvalidEncoding:
                return "InvalidEncoding";
            case ServerErrorCode::InvalidTextFormat:
                return "InvalidTextFormat";

            // Access Errors
            case ServerErrorCode::AccessError:
                return "AccessError";
            case ServerErrorCode::PermissionDenied:
                return "PermissionDenied";
            case ServerErrorCode::ObjectNotFound:
                return "ObjectNotFound";
            case ServerErrorCode::DatabaseNotFound:
                return "DatabaseNotFound";
            case ServerErrorCode::TableNotFound:
                return "TableNotFound";
            case ServerErrorCode::ColumnNotFound:
                return "ColumnNotFound";
            case ServerErrorCode::SchemaNotFound:
                return "SchemaNotFound";
            case ServerErrorCode::FunctionNotFound:
                return "FunctionNotFound";

            // Resource Errors
            case ServerErrorCode::ResourceError:
                return "ResourceError";
            case ServerErrorCode::OutOfMemory:
                return "OutOfMemory";
            case ServerErrorCode::DiskFull:
                return "DiskFull";
            case ServerErrorCode::TooManyConnections:
                return "TooManyConnections";
            case ServerErrorCode::ConfigurationLimit:
                return "ConfigurationLimit";
            case ServerErrorCode::QueryTooComplex:
                return "QueryTooComplex";

            // Transaction Errors
            case ServerErrorCode::TransactionError:
                return "TransactionError";
            case ServerErrorCode::TransactionRollback:
                return "TransactionRollback";
            case ServerErrorCode::TransactionAborted:
                return "TransactionAborted";
            case ServerErrorCode::InvalidIsolationLevel:
                return "InvalidIsolationLevel";
        }
        std::unreachable();
    }

    /**
     * @brief Get human-readable name for FatalErrorCode
     */
    constexpr const char* to_string(const FatalErrorCode code) noexcept {
        switch (code) {
            case FatalErrorCode::InternalError:
                return "InternalError";
            case FatalErrorCode::CorruptionDetected:
                return "CorruptionDetected";
            case FatalErrorCode::ProtocolViolation:
                return "ProtocolViolation";
            case FatalErrorCode::AssertionFailure:
                return "AssertionFailure";
            case FatalErrorCode::UnexpectedState:
                return "UnexpectedState";
        }
        std::unreachable();
    }

    /**
     * @brief Get description for ClientErrorCode
     */
    constexpr const char* get_description(const ClientErrorCode code) noexcept {
        switch (code) {
            case ClientErrorCode::Success:
                return "Operation completed successfully";

            // Invalid Arguments
            case ClientErrorCode::InvalidArgument:
                return "Invalid argument provided to function";
            case ClientErrorCode::SyntaxError:
                return "SQL syntax error in query";
            case ClientErrorCode::InvalidParameter:
                return "Invalid parameter value";
            case ClientErrorCode::TypeMismatch:
                return "Type mismatch between expected and actual value";
            case ClientErrorCode::NullConversion:
                return "Attempted to convert NULL to non-nullable type";
            case ClientErrorCode::InvalidCast:
                return "Invalid type cast operation";
            case ClientErrorCode::OutOfRange:
                return "Value out of valid range";

            // Configuration Errors
            case ClientErrorCode::ConfigurationError:
                return "Database configuration error";
            case ClientErrorCode::ConnectionStringError:
                return "Invalid or malformed connection string";
            case ClientErrorCode::AuthenticationError:
                return "Authentication credentials invalid or missing";
            case ClientErrorCode::InvalidOption:
                return "Invalid configuration option";
            case ClientErrorCode::MissingParameter:
                return "Required parameter is missing";

            // State Errors
            case ClientErrorCode::InvalidState:
                return "Operation invalid in current state";
            case ClientErrorCode::NotConnected:
                return "Not connected to database";
            case ClientErrorCode::AlreadyConnected:
                return "Already connected to database";
            case ClientErrorCode::TransactionActive:
                return "Transaction already active";
            case ClientErrorCode::NoActiveTransaction:
                return "No active transaction";
        }
        std::unreachable();
    }

    /**
     * @brief Get description for ServerErrorCode
     */
    constexpr const char* get_description(const ServerErrorCode code) noexcept {
        switch (code) {
            case ServerErrorCode::Success:
                return "Operation completed successfully";

            // Runtime Errors
            case ServerErrorCode::RuntimeError:
                return "Runtime error on database server";
            case ServerErrorCode::ConnectionError:
                return "Error establishing connection to database";
            case ServerErrorCode::ConnectionLost:
                return "Connection to database was lost";
            case ServerErrorCode::ConnectionTimeout:
                return "Connection attempt timed out";
            case ServerErrorCode::ConnectionRefused:
                return "Connection refused by database server";
            case ServerErrorCode::DeadlockDetected:
                return "Deadlock detected, transaction aborted";
            case ServerErrorCode::LockTimeout:
                return "Timeout waiting for lock";
            case ServerErrorCode::StatementTimeout:
                return "Statement execution timeout";
            case ServerErrorCode::SerializationFailure:
                return "Transaction serialization failure";

            // Constraint Violations
            case ServerErrorCode::ConstraintViolation:
                return "Database constraint violation";
            case ServerErrorCode::UniqueViolation:
                return "Unique constraint violation";
            case ServerErrorCode::ForeignKeyViolation:
                return "Foreign key constraint violation";
            case ServerErrorCode::CheckViolation:
                return "Check constraint violation";
            case ServerErrorCode::NotNullViolation:
                return "NOT NULL constraint violation";
            case ServerErrorCode::ExclusionViolation:
                return "Exclusion constraint violation";

            // Data Errors
            case ServerErrorCode::DataError:
                return "Data error in query or result";
            case ServerErrorCode::DataTooLong:
                return "Data too long for column";
            case ServerErrorCode::NumericOverflow:
                return "Numeric value overflow";
            case ServerErrorCode::DivisionByZero:
                return "Division by zero";
            case ServerErrorCode::InvalidDatetime:
                return "Invalid datetime value";
            case ServerErrorCode::InvalidEncoding:
                return "Invalid character encoding";
            case ServerErrorCode::InvalidTextFormat:
                return "Invalid text representation";

            // Access Errors
            case ServerErrorCode::AccessError:
                return "Database access error";
            case ServerErrorCode::PermissionDenied:
                return "Permission denied for operation";
            case ServerErrorCode::ObjectNotFound:
                return "Database object not found";
            case ServerErrorCode::DatabaseNotFound:
                return "Database does not exist";
            case ServerErrorCode::TableNotFound:
                return "Table does not exist";
            case ServerErrorCode::ColumnNotFound:
                return "Column does not exist";
            case ServerErrorCode::SchemaNotFound:
                return "Schema does not exist";
            case ServerErrorCode::FunctionNotFound:
                return "Function does not exist";

            // Resource Errors
            case ServerErrorCode::ResourceError:
                return "Database resource error";
            case ServerErrorCode::OutOfMemory:
                return "Database server out of memory";
            case ServerErrorCode::DiskFull:
                return "Database disk full";
            case ServerErrorCode::TooManyConnections:
                return "Too many database connections";
            case ServerErrorCode::ConfigurationLimit:
                return "Database configuration limit exceeded";
            case ServerErrorCode::QueryTooComplex:
                return "Query too complex to execute";

            // Transaction Errors
            case ServerErrorCode::TransactionError:
                return "Transaction error";
            case ServerErrorCode::TransactionRollback:
                return "Transaction rolled back";
            case ServerErrorCode::TransactionAborted:
                return "Transaction aborted";
            case ServerErrorCode::InvalidIsolationLevel:
                return "Invalid transaction isolation level";
        }
        std::unreachable();
    }

    /**
     * @brief Get description for FatalErrorCode
     */
    constexpr const char* get_description(const FatalErrorCode code) noexcept {
        switch (code) {
            case FatalErrorCode::InternalError:
                return "Internal database driver error";
            case FatalErrorCode::CorruptionDetected:
                return "Data corruption detected";
            case FatalErrorCode::ProtocolViolation:
                return "Database protocol violation";
            case FatalErrorCode::AssertionFailure:
                return "Internal assertion failed";
            case FatalErrorCode::UnexpectedState:
                return "Unexpected internal state";
        }
        std::unreachable();
    }

    constexpr const char* ErrorCode::name() const noexcept {
        switch (category_) {
            case Category::Success:
                return "Success";
            case Category::Client:
                return to_string(static_cast<ClientErrorCode>(code_));
            case Category::Server:
                return to_string(static_cast<ServerErrorCode>(code_));
            case Category::Fatal:
                return to_string(static_cast<FatalErrorCode>(code_));
        }
        std::unreachable();
    }

    constexpr const char* ErrorCode::description() const noexcept {
        switch (category_) {
            case Category::Success:
                return "Operation completed successfully";
            case Category::Client:
                return get_description(static_cast<ClientErrorCode>(code_));
            case Category::Server:
                return get_description(static_cast<ServerErrorCode>(code_));
            case Category::Fatal:
                return get_description(static_cast<FatalErrorCode>(code_));
        }
        std::unreachable();
    }
}  // namespace demiplane::db
