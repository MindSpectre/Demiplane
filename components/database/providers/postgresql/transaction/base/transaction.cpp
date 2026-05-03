#include "transaction.hpp"

#include <format>

#include <connection_holder.hpp>
#include <savepoint/savepoint.hpp>

#include "detail/identifier_validation.hpp"
#include "log_macros.hpp"

namespace demiplane::db::postgres {

    Transaction::~Transaction() {
        if (auto live = holder_.lock()) {
            if (status_ == TransactionStatus::ACTIVE) {
                COMPONENT_LOG_WRN() << "Transaction destroyed while still active — performing safety-net ROLLBACK";
                PQclear(PQexec(conn_, "ROLLBACK"));
            }
            live->reset();
            COMPONENT_LOG_INF() << "Transaction destroyed, holder released";
        }
    }

    Transaction::Transaction(Transaction&& other) noexcept
        : holder_{std::move(other.holder_)},
          conn_{std::exchange(other.conn_, nullptr)},
          options_{other.options_},
          status_{std::exchange(other.status_, TransactionStatus::FAILED)} {
        other.holder_.reset();
    }

    Transaction& Transaction::operator=(Transaction&& other) noexcept {
        if (this != &other) {
            if (auto live = holder_.lock()) {
                live->reset();
            }
            holder_  = std::move(other.holder_);
            conn_    = std::exchange(other.conn_, nullptr);
            options_ = other.options_;
            status_  = std::exchange(other.status_, TransactionStatus::FAILED);
            other.holder_.reset();
        }
        return *this;
    }

    gears::Outcome<void, ErrorContext> Transaction::begin() {
        COMPONENT_LOG_ENTER_FUNCTION();
        if (status_ != TransactionStatus::IDLE) {
            return gears::err(ErrorContext{ErrorCode{ClientErrorCode::InvalidState}});
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
            return gears::err(ErrorContext{ErrorCode{ClientErrorCode::InvalidState}});
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
            return gears::err(ErrorContext{ErrorCode{ClientErrorCode::InvalidState}});
        }
        auto result = execute_control("ROLLBACK");
        if (result.is_success()) {
            status_ = TransactionStatus::ROLLED_BACK;
        } else {
            status_ = TransactionStatus::FAILED;
        }
        return result;
    }

    gears::Outcome<SyncExecutor, ErrorContext> Transaction::with_sync() const {
        if (status_ != TransactionStatus::ACTIVE) {
            return gears::err(ErrorContext{ErrorCode{ClientErrorCode::InvalidState}});
        }
        return SyncExecutor{conn_};
    }

    gears::Outcome<AsyncExecutor, ErrorContext> Transaction::with_async(boost::asio::any_io_executor exec) const {
        if (status_ != TransactionStatus::ACTIVE) {
            return gears::err(ErrorContext{ErrorCode{ClientErrorCode::InvalidState}});
        }
        return AsyncExecutor{conn_, std::move(exec)};
    }

    gears::Outcome<Savepoint, ErrorContext> Transaction::savepoint(std::string name) const {
        COMPONENT_LOG_ENTER_FUNCTION();
        if (status_ != TransactionStatus::ACTIVE) {
            return gears::err(ErrorContext{ErrorCode{ClientErrorCode::InvalidState}});
        }
        if (!is_valid_identifier(name)) {
            auto err    = ErrorContext{ErrorCode{ClientErrorCode::InvalidArgument}};
            err.message = std::format("Invalid savepoint name: '{}'", name);
            return gears::err(std::move(err));
        }
        const std::string sql = std::format(R"(SAVEPOINT "{}")", name);
        if (auto result = execute_control(sql); !result.is_success()) {
            return gears::err(result.error<ErrorContext>());
        }
        return Savepoint{conn_, std::move(name)};
    }


    Transaction::Transaction(std::weak_ptr<ConnectionHolder> holder, const TransactionOptions opts)
        : holder_{std::move(holder)},
          options_{opts} {
        if (auto live = holder_.lock()) {
            conn_ = live->conn();
        }
        COMPONENT_LOG_INF() << "Transaction created";
    }

    gears::Outcome<void, ErrorContext> Transaction::execute_control(const std::string& sql) const {
        const SyncExecutor inner{conn_};
        if (auto result = inner.execute(sql); !result.is_success()) {
            return gears::err(result.error<ErrorContext>());
        }
        return gears::ok();
    }

}  // namespace demiplane::db::postgres
