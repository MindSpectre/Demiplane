#include "postgres_session.hpp"

#include <db_error_codes.hpp>

#include "log_macros.hpp"

namespace demiplane::db::postgres {

    Session::Session(ConnectionConfig connection_config, PoolConfig pool_config)
        : pool_{std::move(connection_config), std::move(pool_config)},
          janitor_{pool_} {
        COMPONENT_LOG_INF() << "Session created";
    }

    Session::~Session() {
        shutdown();
        COMPONENT_LOG_INF() << "Session destroyed";
    }

    gears::Outcome<SyncExecutor, ErrorContext> Session::with_sync() {
        COMPONENT_LOG_ENTER_FUNCTION();
        auto* slot = pool_.acquire_slot();
        if (!slot) {
            return gears::Err(ErrorContext{ErrorCode{ClientErrorCode::PoolExhausted}});
        }
        return SyncExecutor{*slot};
    }

    gears::Outcome<AsyncExecutor, ErrorContext> Session::with_async(boost::asio::any_io_executor exec) {
        COMPONENT_LOG_ENTER_FUNCTION();
        auto* slot = pool_.acquire_slot();
        if (!slot) {
            return gears::Err(ErrorContext{ErrorCode{ClientErrorCode::PoolExhausted}});
        }
        return AsyncExecutor{*slot, std::move(exec)};
    }

    void Session::shutdown() {
        COMPONENT_LOG_ENTER_FUNCTION();
        janitor_.stop();
        pool_.shutdown();
        COMPONENT_LOG_LEAVE_FUNCTION();
    }

    std::size_t Session::pool_capacity() const noexcept {
        return pool_.capacity();
    }

    std::size_t Session::pool_active_count() const noexcept {
        return pool_.active_count();
    }

    std::size_t Session::pool_free_count() const noexcept {
        return pool_.free_count();
    }

    bool Session::is_shutdown() const noexcept {
        return pool_.is_shutdown();
    }

    gears::Outcome<Transaction, ErrorContext> Session::begin_transaction(const TransactionOptions opts) {
        COMPONENT_LOG_ENTER_FUNCTION();
        auto* slot = pool_.acquire_slot();
        if (!slot) {
            return gears::Err(ErrorContext{ErrorCode{ClientErrorCode::PoolExhausted}});
        }
        return Transaction{*slot, opts};
    }

    gears::Outcome<AutoTransaction, ErrorContext> Session::begin_auto_transaction(const TransactionOptions opts) {
        COMPONENT_LOG_ENTER_FUNCTION();
        auto* slot = pool_.acquire_slot();
        if (!slot) {
            return gears::Err(ErrorContext{ErrorCode{ClientErrorCode::PoolExhausted}});
        }

        Transaction tx{*slot, opts};
        if (auto begin_result = tx.begin(); !begin_result.is_success()) {
            return gears::Err(begin_result.error<ErrorContext>());
        }

        return AutoTransaction{std::move(tx)};
    }

}  // namespace demiplane::db::postgres
