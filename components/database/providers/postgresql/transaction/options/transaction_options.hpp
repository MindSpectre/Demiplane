#pragma once

#include <cstdint>
#include <string>

namespace demiplane::db::postgres {

    enum class IsolationLevel : std::uint8_t {
        READ_COMMITTED,
        REPEATABLE_READ,
        SERIALIZABLE,
    };

    enum class AccessMode : std::uint8_t {
        READ_WRITE,
        READ_ONLY,
    };

    [[nodiscard]] constexpr const char* to_string(const IsolationLevel level) noexcept {
        switch (level) {
            case IsolationLevel::READ_COMMITTED:
                return "READ COMMITTED";
            case IsolationLevel::REPEATABLE_READ:
                return "REPEATABLE READ";
            case IsolationLevel::SERIALIZABLE:
                return "SERIALIZABLE";
        }
        return "UNKNOWN";
    }

    [[nodiscard]] constexpr const char* to_string(const AccessMode mode) noexcept {
        switch (mode) {
            case AccessMode::READ_WRITE:
                return "READ WRITE";
            case AccessMode::READ_ONLY:
                return "READ ONLY";
        }
        return "UNKNOWN";
    }

    struct TransactionOptions {
        IsolationLevel isolation = IsolationLevel::READ_COMMITTED;
        AccessMode access        = AccessMode::READ_WRITE;
        bool deferrable          = false;

        [[nodiscard]] std::string to_begin_sql() const;
    };

}  // namespace demiplane::db::postgres
