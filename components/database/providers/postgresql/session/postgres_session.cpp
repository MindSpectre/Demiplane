#include "postgres_session.hpp"

#include <db_error_codes.hpp>

#include "log_macros.hpp"

namespace demiplane::db::postgres {

    Session::Session(ConnectionConfig connection_config, CylinderConfig cylinder_config)
        : cylinder_{std::move(connection_config), std::move(cylinder_config)},
          janitor_{cylinder_} {
        COMPONENT_LOG_INF() << "Session created";
    }

    Session::~Session() {
        shutdown();
        COMPONENT_LOG_INF() << "Session destroyed";
    }

    SyncExecutor Session::with_sync() {
        COMPONENT_LOG_ENTER_FUNCTION();
        auto* slot = cylinder_.acquire_slot();
        if (!slot) {
            return SyncExecutor{nullptr};
        }
        return SyncExecutor{*slot};
    }

    AsyncExecutor Session::with_async(executor_type exec) {
        COMPONENT_LOG_ENTER_FUNCTION();
        auto* slot = cylinder_.acquire_slot();
        if (!slot) {
            return AsyncExecutor{nullptr, std::move(exec)};
        }
        return AsyncExecutor{*slot, std::move(exec)};
    }

    void Session::shutdown() {
        COMPONENT_LOG_ENTER_FUNCTION();
        janitor_.stop();
        cylinder_.shutdown();
        COMPONENT_LOG_LEAVE_FUNCTION();
    }

    std::size_t Session::cylinder_capacity() const noexcept {
        return cylinder_.capacity();
    }

    std::size_t Session::cylinder_active_count() const noexcept {
        return cylinder_.active_count();
    }

    std::size_t Session::cylinder_free_count() const noexcept {
        return cylinder_.free_count();
    }

    bool Session::is_shutdown() const noexcept {
        return cylinder_.is_shutdown();
    }

    gears::Outcome<Transaction, ErrorContext> Session::begin_transaction(TransactionOptions opts) {
        COMPONENT_LOG_ENTER_FUNCTION();
        auto* slot = cylinder_.acquire_slot();
        if (!slot) {
            return gears::Err(ErrorContext{ErrorCode{ClientErrorCode::PoolExhausted}});
        }
        return Transaction{*slot, std::move(opts)};
    }

    gears::Outcome<AutoTransaction, ErrorContext> Session::begin_auto_transaction(TransactionOptions opts) {
        COMPONENT_LOG_ENTER_FUNCTION();
        auto* slot = cylinder_.acquire_slot();
        if (!slot) {
            return gears::Err(ErrorContext{ErrorCode{ClientErrorCode::PoolExhausted}});
        }

        Transaction tx{*slot, std::move(opts)};
        auto begin_result = tx.begin();
        if (!begin_result.is_success()) {
            return gears::Err(begin_result.error<ErrorContext>());
        }

        return AutoTransaction{std::move(tx)};
    }

}  // namespace demiplane::db::postgres
