#pragma once

#include <chrono>

#include <capability_provider.hpp>
#include <gears_class_traits.hpp>
#include <gears_outcome.hpp>
#include <postgres_errors.hpp>
#include <postgres_transaction.hpp>

#include "pool/connection_pool.hpp"
#include "pool/pool_janitor.hpp"

namespace demiplane::db::postgres {

    /**
     * @brief PostgreSQL session owning a connection pool and background janitor
     *
     * The Session is the primary entry point for database operations.
     * Each with_sync() / with_async() call acquires a slot from the pool
     * and returns a slot-aware executor that resets the slot on destruction.
     *
     * Usage:
     *   auto session = Session(conn_config, pool_config);
     *
     *   // Synchronous
     *   auto result = session.with_sync().execute("SELECT 1");
     *
     *   // Asynchronous
     *   auto result = co_await session.with_async(io_exec).execute("SELECT 1");
     */
    class LockFreeSession : gears::Immutable {
    public:
        LockFreeSession(ConnectionConfig connection_config, PoolConfig pool_config);
        ~LockFreeSession();

        /**
         * @brief Get a synchronous executor with an acquired connection
         * @return SyncExecutor on success, ErrorContext on pool exhaustion
         */
        [[nodiscard]] gears::Outcome<SyncExecutor, ErrorContext> with_sync();

        /**
         * @brief Get a synchronous executor, waiting up to `timeout` for a free slot
         * @param timeout Duration to wait if the pool is exhausted on the first attempt.
         *                Zero returns immediately (same as the no-arg overload).
         * @return SyncExecutor on success, ErrorContext{PoolExhausted} on full timeout
         */
        [[nodiscard]] gears::Outcome<SyncExecutor, ErrorContext> with_sync(std::chrono::steady_clock::duration timeout);

        /**
         * @brief Get an asynchronous executor with an acquired connection
         * @param exec Boost.Asio executor for async I/O
         * @return AsyncExecutor on success, ErrorContext on pool exhaustion
         */
        [[nodiscard]] gears::Outcome<AsyncExecutor, ErrorContext> with_async(boost::asio::any_io_executor exec);

        /**
         * @brief Get an async executor, waiting up to `timeout` for a free slot
         * @param exec Boost.Asio executor for async I/O
         * @param timeout Duration to wait if the pool is exhausted on the first attempt.
         *                Zero returns immediately (same as the single-arg overload).
         * @return AsyncExecutor on success, ErrorContext{PoolExhausted} on full timeout
         *
         * Note: the timeout wait blocks the calling thread. An awaitable variant
         * that suspends the coroutine may be added later.
         */
        [[nodiscard]] gears::Outcome<AsyncExecutor, ErrorContext>
        with_async(boost::asio::any_io_executor exec, std::chrono::steady_clock::duration timeout);

        /**
         * @brief Shutdown the session: stop janitor, drain pool
         */
        void shutdown();

        // ============== Transaction Factories ==============

        [[nodiscard]] gears::Outcome<Transaction, ErrorContext> begin_transaction(TransactionOptions opts = {});

        /**
         * @brief Begin a transaction, waiting up to `timeout` for a free slot
         * @param opts Transaction isolation / read-only / deferrable options
         * @param timeout Duration to wait if the pool is exhausted on the first attempt.
         * @return Transaction on success, ErrorContext{PoolExhausted} on full timeout
         */
        [[nodiscard]] gears::Outcome<Transaction, ErrorContext>
        begin_transaction(TransactionOptions opts, std::chrono::steady_clock::duration timeout);

        [[nodiscard]] gears::Outcome<AutoTransaction, ErrorContext>
        begin_auto_transaction(TransactionOptions opts = {});

        /**
         * @brief Begin an auto-transaction, waiting up to `timeout` for a free slot
         * @param opts Transaction isolation / read-only / deferrable options
         * @param timeout Duration to wait if the pool is exhausted on the first attempt.
         * @return AutoTransaction on success, ErrorContext{PoolExhausted} on full timeout
         *         or on failure of the implicit BEGIN
         */
        [[nodiscard]] gears::Outcome<AutoTransaction, ErrorContext>
        begin_auto_transaction(TransactionOptions opts, std::chrono::steady_clock::duration timeout);

        // ============== Pool Stats ==============

        [[nodiscard]] std::size_t pool_capacity() const noexcept;

        [[nodiscard]] std::size_t pool_active_count() const noexcept;

        [[nodiscard]] std::size_t pool_free_count() const noexcept;

        [[nodiscard]] bool is_shutdown() const noexcept;

    private:
        ConnectionPool pool_;
        PoolJanitor janitor_;
    };

    static_assert(CapabilityProvider<LockFreeSession>);
}  // namespace demiplane::db::postgres
