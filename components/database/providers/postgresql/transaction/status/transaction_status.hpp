#pragma once

#include <cstdint>

namespace demiplane::db::postgres {

    enum class TransactionStatus : std::uint8_t {
        IDLE,         // Created but begin() not yet called
        ACTIVE,       // BEGIN sent, queries can execute
        COMMITTED,    // COMMIT sent successfully
        ROLLED_BACK,  // ROLLBACK sent successfully
        FAILED,       // A control command failed
    };

    [[nodiscard]] constexpr const char* to_string(const TransactionStatus status) noexcept {
        switch (status) {
            case TransactionStatus::IDLE:
                return "IDLE";
            case TransactionStatus::ACTIVE:
                return "ACTIVE";
            case TransactionStatus::COMMITTED:
                return "COMMITTED";
            case TransactionStatus::ROLLED_BACK:
                return "ROLLED_BACK";
            case TransactionStatus::FAILED:
                return "FAILED";
        }
        return "UNKNOWN";
    }

}  // namespace demiplane::db::postgres
