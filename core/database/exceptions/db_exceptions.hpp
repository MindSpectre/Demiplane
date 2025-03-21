#pragma once

#include <stdexcept>
#include <string>

#include "db_error_codes.hpp"

namespace demiplane::database::exceptions {
    class DatabaseException : public std::runtime_error {
    protected:
        errors::db_error_code m_error_code = errors::db_error_code::UNKNOWN_ERROR;

    public:
        explicit DatabaseException(const std::string& message, const errors::db_error_code error_code)
            : std::runtime_error(message), m_error_code(error_code) {}

        [[nodiscard]] errors::db_error_code get_error() const {
            return m_error_code;
        }
    };

    class ConnectionException final : public DatabaseException {
    public:
        explicit ConnectionException(const std::string& message, const errors::db_error_code code)
            : DatabaseException("ConnectionException: " + message, code) {}
    };

    class QueryException final : public DatabaseException {
    public:
        explicit QueryException(const std::string& message, const errors::db_error_code code)
            : DatabaseException("QueryException: " + message, code) {}
    };

    class TransactionException final : public DatabaseException {
    public:
        explicit TransactionException(const std::string& message, const errors::db_error_code code)
            : DatabaseException("TransactionException: " + message, code) {}
    };

    class InvalidIdentifierException final : public DatabaseException {
    public:
        explicit InvalidIdentifierException(const std::string& message, const errors::db_error_code code)
            : DatabaseException("InvalidIdentifierException: " + message, code) {}
    };

    // Additional exceptions can be added here
} // namespace demiplane::database::exceptions
