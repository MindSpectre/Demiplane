#include "postgres_blocking_session.hpp"

#include <utility>

#include <boost/asio/as_tuple.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <db_error_codes.hpp>

#include "log_macros.hpp"

namespace demiplane::db::postgres {

    namespace {
        ClientErrorCode map_empty_weak_timed(const BlockingPool& pool) noexcept {
            return pool.is_shutdown() ? ClientErrorCode::PoolShutdown : ClientErrorCode::WaitTimeout;
        }
    }  // namespace

    BlockingSession::BlockingSession(ConnectionConfig connection_config, PoolConfig pool_config)
        : pool_{std::move(connection_config), std::move(pool_config)} {
        COMPONENT_LOG_INF() << "BlockingSession created";
    }

    BlockingSession::~BlockingSession() {
        shutdown();
        COMPONENT_LOG_INF() << "BlockingSession destroyed";
    }

    // ============== Sync ==============

    gears::Outcome<SyncExecutor, ErrorContext> BlockingSession::try_with_sync() noexcept {
        auto holder = pool_.try_acquire();
        if (holder.expired()) {
            return gears::Err(ErrorContext{ErrorCode{ClientErrorCode::PoolExhausted}});
        }
        return SyncExecutor{std::move(holder)};
    }

    gears::Outcome<SyncExecutor, ErrorContext>
    BlockingSession::with_sync(std::chrono::steady_clock::duration timeout) noexcept {
        auto holder = pool_.acquire(timeout);
        if (holder.expired()) {
            return gears::Err(ErrorContext{ErrorCode{map_empty_weak_timed(pool_)}});
        }
        return SyncExecutor{std::move(holder)};
    }

    gears::Outcome<SyncExecutor, ErrorContext> BlockingSession::with_sync() noexcept {
        auto holder = pool_.acquire();
        if (holder.expired()) {
            return gears::Err(ErrorContext{ErrorCode{ClientErrorCode::PoolShutdown}});
        }
        return SyncExecutor{std::move(holder)};
    }

    // ============== Async ==============

    gears::Outcome<AsyncExecutor, ErrorContext>
    BlockingSession::try_with_async(boost::asio::any_io_executor exec) noexcept {
        auto holder = pool_.try_acquire();
        if (holder.expired()) {
            return gears::Err(ErrorContext{ErrorCode{ClientErrorCode::PoolExhausted}});
        }
        return AsyncExecutor{std::move(holder), std::move(exec)};
    }

    boost::asio::awaitable<gears::Outcome<AsyncExecutor, ErrorContext>>
    BlockingSession::with_async(boost::asio::any_io_executor exec, std::chrono::steady_clock::duration timeout) {
        auto [ec, holder_sp] =
            co_await pool_.async_acquire(exec, timeout, boost::asio::as_tuple(boost::asio::use_awaitable));

        if (ec == boost::asio::error::operation_aborted) {
            co_return gears::Err(ErrorContext{ErrorCode{ClientErrorCode::PoolShutdown}});
        }
        if (ec) {
            co_return gears::Err(ErrorContext{ErrorCode{ClientErrorCode::WaitTimeout}});
        }
        if (!holder_sp) {
            co_return gears::Err(ErrorContext{ErrorCode{ClientErrorCode::PoolShutdown}});
        }

        co_return AsyncExecutor{std::weak_ptr<ConnectionHolder>{holder_sp}, std::move(exec)};
    }

    boost::asio::awaitable<gears::Outcome<AsyncExecutor, ErrorContext>>
    BlockingSession::with_async(boost::asio::any_io_executor exec) {
        auto [ec, holder_sp] = co_await pool_.async_acquire(exec, boost::asio::as_tuple(boost::asio::use_awaitable));

        if (ec || !holder_sp) {
            co_return gears::Err(ErrorContext{ErrorCode{ClientErrorCode::PoolShutdown}});
        }

        co_return AsyncExecutor{std::weak_ptr<ConnectionHolder>{holder_sp}, std::move(exec)};
    }

    // ============== Transactions ==============

    gears::Outcome<Transaction, ErrorContext> BlockingSession::try_begin_transaction(TransactionOptions opts) noexcept {
        auto holder = pool_.try_acquire();
        if (holder.expired()) {
            return gears::Err(ErrorContext{ErrorCode{ClientErrorCode::PoolExhausted}});
        }
        return Transaction{std::move(holder), opts};
    }

    gears::Outcome<Transaction, ErrorContext>
    BlockingSession::begin_transaction(TransactionOptions opts, std::chrono::steady_clock::duration timeout) noexcept {
        auto holder = pool_.acquire(timeout);
        if (holder.expired()) {
            return gears::Err(ErrorContext{ErrorCode{map_empty_weak_timed(pool_)}});
        }
        return Transaction{std::move(holder), opts};
    }

    gears::Outcome<Transaction, ErrorContext> BlockingSession::begin_transaction(TransactionOptions opts) noexcept {
        auto holder = pool_.acquire();
        if (holder.expired()) {
            return gears::Err(ErrorContext{ErrorCode{ClientErrorCode::PoolShutdown}});
        }
        return Transaction{std::move(holder), opts};
    }

    gears::Outcome<AutoTransaction, ErrorContext>
    BlockingSession::try_begin_auto_transaction(TransactionOptions opts) noexcept {
        auto tx_outcome = try_begin_transaction(opts);
        if (!tx_outcome.is_success()) {
            return gears::Err(tx_outcome.error<ErrorContext>());
        }
        Transaction tx = std::move(tx_outcome).value();
        if (auto b = tx.begin(); !b.is_success()) {
            return gears::Err(b.error<ErrorContext>());
        }
        return AutoTransaction{std::move(tx)};
    }

    gears::Outcome<AutoTransaction, ErrorContext>
    BlockingSession::begin_auto_transaction(TransactionOptions opts,
                                            std::chrono::steady_clock::duration timeout) noexcept {
        auto tx_outcome = begin_transaction(opts, timeout);
        if (!tx_outcome.is_success()) {
            return gears::Err(tx_outcome.error<ErrorContext>());
        }
        Transaction tx = std::move(tx_outcome).value();
        if (auto b = tx.begin(); !b.is_success()) {
            return gears::Err(b.error<ErrorContext>());
        }
        return AutoTransaction{std::move(tx)};
    }

    gears::Outcome<AutoTransaction, ErrorContext>
    BlockingSession::begin_auto_transaction(TransactionOptions opts) noexcept {
        auto tx_outcome = begin_transaction(opts);
        if (!tx_outcome.is_success()) {
            return gears::Err(tx_outcome.error<ErrorContext>());
        }
        Transaction tx = std::move(tx_outcome).value();
        if (auto b = tx.begin(); !b.is_success()) {
            return gears::Err(b.error<ErrorContext>());
        }
        return AutoTransaction{std::move(tx)};
    }

    // ============== Lifecycle + Stats ==============

    void BlockingSession::shutdown() {
        pool_.shutdown();
    }

    std::size_t BlockingSession::pool_capacity() const noexcept {
        return pool_.capacity();
    }
    std::size_t BlockingSession::pool_active_count() const noexcept {
        return pool_.active_count();
    }
    std::size_t BlockingSession::pool_free_count() const noexcept {
        return pool_.free_count();
    }
    std::size_t BlockingSession::pool_waiter_count() const noexcept {
        return pool_.waiter_count();
    }
    bool BlockingSession::is_shutdown() const noexcept {
        return pool_.is_shutdown();
    }

}  // namespace demiplane::db::postgres
