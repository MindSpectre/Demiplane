#pragma once

#include <chrono>

#include <boost/asio/awaitable.hpp>
#include <capability_provider.hpp>
#include <gears_class_traits.hpp>
#include <gears_outcome.hpp>
#include <postgres_errors.hpp>
#include <postgres_transaction.hpp>

#include "pool/blocking_pool.hpp"

namespace demiplane::db::postgres {

    /**
     * PostgreSQL session backed by a classic mutex + CV blocking pool.
     *
     * Unlike Session (lock-free, fail-fast), BlockingSession offers strict
     * FIFO waiter fairness, condition-variable wakeups, and three
     * acquisition modes per method: try (non-blocking, fails on exhaustion),
     * timed (bounded wait), and blocking (unbounded until success/shutdown).
     */
    class BlockingSession : gears::Immutable {
    public:
        BlockingSession(ConnectionConfig connection_config, PoolConfig pool_config);
        ~BlockingSession();

        // ============== Sync Executor ==============

        [[nodiscard]] gears::Outcome<SyncExecutor, ErrorContext> try_with_sync() noexcept;
        [[nodiscard]] gears::Outcome<SyncExecutor, ErrorContext>
        with_sync(std::chrono::steady_clock::duration timeout) noexcept;
        [[nodiscard]] gears::Outcome<SyncExecutor, ErrorContext> with_sync() noexcept;

        // ============== Async Executor ==============

        [[nodiscard]] gears::Outcome<AsyncExecutor, ErrorContext>
        try_with_async(boost::asio::any_io_executor exec) noexcept;

        // Coroutine-aware acquire. Never blocks the calling thread; suspends
        // the caller until a slot is available or the timeout expires.
        [[nodiscard]] boost::asio::awaitable<gears::Outcome<AsyncExecutor, ErrorContext>>
        with_async(boost::asio::any_io_executor exec, std::chrono::steady_clock::duration timeout);

        // Unbounded async acquire. Suspends until a slot is available or the
        // pool shuts down.
        [[nodiscard]] boost::asio::awaitable<gears::Outcome<AsyncExecutor, ErrorContext>>
        with_async(boost::asio::any_io_executor exec);

        // ============== Transactions ==============

        [[nodiscard]] gears::Outcome<Transaction, ErrorContext>
        try_begin_transaction(TransactionOptions opts = {}) noexcept;
        [[nodiscard]] gears::Outcome<Transaction, ErrorContext>
        begin_transaction(TransactionOptions opts, std::chrono::steady_clock::duration timeout) noexcept;
        [[nodiscard]] gears::Outcome<Transaction, ErrorContext>
        begin_transaction(TransactionOptions opts = {}) noexcept;

        [[nodiscard]] gears::Outcome<AutoTransaction, ErrorContext>
        try_begin_auto_transaction(TransactionOptions opts = {}) noexcept;
        [[nodiscard]] gears::Outcome<AutoTransaction, ErrorContext>
        begin_auto_transaction(TransactionOptions opts, std::chrono::steady_clock::duration timeout) noexcept;
        [[nodiscard]] gears::Outcome<AutoTransaction, ErrorContext>
        begin_auto_transaction(TransactionOptions opts = {}) noexcept;

        // ============== Lifecycle + Stats ==============

        void shutdown();

        [[nodiscard]] std::size_t pool_capacity() const noexcept;
        [[nodiscard]] std::size_t pool_active_count() const noexcept;
        [[nodiscard]] std::size_t pool_free_count() const noexcept;
        [[nodiscard]] std::size_t pool_waiter_count() const noexcept;
        [[nodiscard]] bool is_shutdown() const noexcept;

    private:
        BlockingPool pool_;
    };

    // NOTE: BlockingSession does not satisfy the CapabilityProvider concept
    // because with_async(exec) now returns an awaitable instead of a
    // synchronous Outcome — it is the one session type whose async acquire
    // path suspends a coroutine rather than blocking the calling thread.
    // LockFreeSession still satisfies CapabilityProvider.

}  // namespace demiplane::db::postgres
