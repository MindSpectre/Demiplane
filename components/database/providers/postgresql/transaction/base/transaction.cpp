#include "transaction.hpp"

#include <format>

#include <connection_slot.hpp>
#include <savepoint/savepoint.hpp>

#include "detail/identifier_validation.hpp"
#include "log_macros.hpp"

namespace demiplane::db::postgres {

    Transaction::~Transaction() {
        if (slot_) {
            if (status_ == TransactionStatus::ACTIVE) {
                COMPONENT_LOG_WRN() << "Transaction destroyed while still active — performing safety-net ROLLBACK";
                PQclear(PQexec(slot_->conn, "ROLLBACK"));
            }
            slot_->reset();
            COMPONENT_LOG_INF() << "Transaction destroyed, slot released";
        }
    }

    Transaction::Transaction(Transaction&& other) noexcept
        : slot_{std::exchange(other.slot_, nullptr)},
          options_{other.options_},
          status_{std::exchange(other.status_, TransactionStatus::FAILED)} {
    }

    Transaction& Transaction::operator=(Transaction&& other) noexcept {
        if (this != &other) {
            if (slot_) {
                slot_->reset();
            }
            slot_    = std::exchange(other.slot_, nullptr);
            options_ = other.options_;
            status_  = std::exchange(other.status_, TransactionStatus::FAILED);
        }
        return *this;
    }

    gears::Outcome<void, ErrorContext> Transaction::begin() {
        COMPONENT_LOG_ENTER_FUNCTION();
        if (status_ != TransactionStatus::IDLE) {
            return gears::Err(ErrorContext{ErrorCode{ClientErrorCode::InvalidState}});
        }
        auto result = execute_control(options_.to_begin_sql());
        if (result.is_success()) {
            status_ = TransactionStatus::ACTIVE;
        } else {
            status_ = TransactionStatus::FAILED;
        }
        return result;
    }

    gears::Outcome<void, ErrorContext> Transaction::commit() {
        COMPONENT_LOG_ENTER_FUNCTION();
        if (status_ != TransactionStatus::ACTIVE) {
            return gears::Err(ErrorContext{ErrorCode{ClientErrorCode::InvalidState}});
        }
        auto result = execute_control("COMMIT");
        if (result.is_success()) {
            status_ = TransactionStatus::COMMITTED;
        } else {
            status_ = TransactionStatus::FAILED;
        }
        return result;
    }

    gears::Outcome<void, ErrorContext> Transaction::rollback() {
        COMPONENT_LOG_ENTER_FUNCTION();
        if (status_ != TransactionStatus::ACTIVE) {
            return gears::Err(ErrorContext{ErrorCode{ClientErrorCode::InvalidState}});
        }
        auto result = execute_control("ROLLBACK");
        if (result.is_success()) {
            status_ = TransactionStatus::ROLLED_BACK;
        } else {
            status_ = TransactionStatus::FAILED;
        }
        return result;
    }

    SyncExecutor Transaction::with_sync() const {
        if (status_ != TransactionStatus::ACTIVE) {
            return SyncExecutor{nullptr};
        }
        return SyncExecutor{slot_->conn};
    }

    AsyncExecutor Transaction::with_async(boost::asio::any_io_executor exec) const {
        if (status_ != TransactionStatus::ACTIVE) {
            return AsyncExecutor{nullptr, std::move(exec)};
        }
        return AsyncExecutor{slot_->conn, std::move(exec)};
    }

    gears::Outcome<Savepoint, ErrorContext> Transaction::savepoint(std::string name) const {
        COMPONENT_LOG_ENTER_FUNCTION();
        if (status_ != TransactionStatus::ACTIVE) {
            return gears::Err(ErrorContext{ErrorCode{ClientErrorCode::InvalidState}});
        }
        if (!is_valid_identifier(name)) {
            auto err    = ErrorContext{ErrorCode{ClientErrorCode::InvalidArgument}};
            err.message = std::format("Invalid savepoint name: '{}'", name);
            return gears::Err(std::move(err));
        }
        const std::string sql = std::format(R"(SAVEPOINT "{}")", name);
        if (auto result = execute_control(sql); !result.is_success()) {
            return gears::Err(result.error<ErrorContext>());
        }
        return Savepoint{slot_->conn, std::move(name)};
    }


    Transaction::Transaction(ConnectionSlot& slot, const TransactionOptions opts)
        : slot_{&slot},
          options_{opts} {
        COMPONENT_LOG_INF() << "Transaction created";
    }

    gears::Outcome<void, ErrorContext> Transaction::execute_control(const std::string& sql) const {
        const SyncExecutor exec{slot_->conn};
        if (auto result = exec.execute(sql); !result.is_success()) {
            return gears::Err(result.error<ErrorContext>());
        }
        return gears::Ok();
    }

}  // namespace demiplane::db::postgres
