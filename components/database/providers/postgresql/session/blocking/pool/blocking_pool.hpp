#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <deque>
#include <memory>
#include <mutex>
#include <utility>
#include <variant>
#include <vector>

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/async_result.hpp>
#include <connection_holder.hpp>
#include <gears_class_traits.hpp>
#include <pool_config.hpp>
#include <postgres_connection_config.hpp>

#include "async_waiter.hpp"
#include "queued_holder.hpp"
#include "waiter.hpp"

namespace demiplane::db::postgres {

    /**
     * Classic mutex + FIFO-waiter PostgreSQL connection pool.
     *
     * Owns its QueuedHolders as std::shared_ptr; capabilities receive
     * std::weak_ptr<ConnectionHolder>. On release, either hands the
     * connection directly to the head waiter (under the pool mutex) or
     * pushes it onto the free deque.
     *
     * Acquisition modes:
     *   - try_acquire(): non-blocking; fails fast on exhaustion
     *   - acquire(timeout): bounded CV wait; blocks the calling thread
     *   - acquire(): unbounded CV wait; blocks the calling thread
     *   - async_acquire(exec, timeout, token): asio-compatible async
     *     operation; suspends the caller's coroutine without ever
     *     blocking an io_context worker thread
     *   - async_acquire(exec, token): unbounded async variant
     *
     * Sync (Waiter) and async (AsyncWaiter) waiters share a single FIFO
     * deque so fairness is preserved regardless of which acquisition
     * mode each caller uses.
     */
    class BlockingPool : gears::Immutable {
    public:
        using AsyncSignature = void(boost::system::error_code, std::shared_ptr<QueuedHolder>);

        BlockingPool(ConnectionConfig connection_config, PoolConfig pool_config);
        ~BlockingPool();

        [[nodiscard]] std::weak_ptr<ConnectionHolder> try_acquire() noexcept;

        [[nodiscard]] std::weak_ptr<ConnectionHolder> acquire(std::chrono::steady_clock::duration timeout) noexcept;

        [[nodiscard]] std::weak_ptr<ConnectionHolder> acquire() noexcept;

        template <typename CompletionToken>
        auto async_acquire(const boost::asio::any_io_executor& exec,
                           std::chrono::steady_clock::duration timeout,
                           CompletionToken&& token) {
            return boost::asio::async_initiate<CompletionToken, AsyncSignature>(
                [this, exec, timeout]<typename T>(T&& raw_handler) mutable {
                    this->initiate_async_acquire(exec, timeout, AsyncWaiter::Handler{std::forward<T>(raw_handler)});
                },
                token);
        }

        template <typename CompletionToken>
        auto async_acquire(const boost::asio::any_io_executor& exec, CompletionToken&& token) {
            return boost::asio::async_initiate<CompletionToken, AsyncSignature>(
                [this, exec]<typename T>(T&& raw_handler) mutable {
                    this->initiate_async_acquire(exec,
                                                 std::chrono::steady_clock::duration::zero(),
                                                 AsyncWaiter::Handler{std::forward<T>(raw_handler)});
                },
                token);
        }

        void shutdown();

        [[nodiscard]] std::size_t capacity() const noexcept;
        [[nodiscard]] std::size_t free_count() const noexcept;
        [[nodiscard]] std::size_t active_count() const noexcept;
        [[nodiscard]] std::size_t waiter_count() const noexcept;
        [[nodiscard]] bool is_shutdown() const noexcept;

        [[nodiscard]] const ConnectionConfig& connection_config() const noexcept;
        [[nodiscard]] const PoolConfig& pool_config() const noexcept;

        [[nodiscard]] PGconn* create_connection() const;

    private:
        friend class QueuedHolder;
        void return_holder(QueuedHolder* raw) noexcept;
        void drop_dead(QueuedHolder* raw) noexcept;

        [[nodiscard]] std::shared_ptr<QueuedHolder> try_create_holder();

        // Shared deque of sync and async waiters, in FIFO arrival order.
        using WaiterEntry = std::variant<Waiter*, std::shared_ptr<AsyncWaiter>>;

        // Non-template initiation body. Zero timeout means "unbounded".
        void initiate_async_acquire(const boost::asio::any_io_executor& exec,
                                    std::chrono::steady_clock::duration timeout,
                                    AsyncWaiter::Handler handler);

        // Called from an AsyncWaiter's timer callback.
        void on_async_waiter_timeout(const std::shared_ptr<AsyncWaiter>& waiter) noexcept;

        ConnectionConfig conn_cfg_;
        PoolConfig pool_cfg_;

        mutable std::mutex mtx_;
        std::vector<std::shared_ptr<QueuedHolder>> holders_;
        std::deque<QueuedHolder*> free_;
        std::deque<WaiterEntry> waiters_;
        std::size_t created_ = 0;

        std::atomic_bool shutdown_{false};
    };

}  // namespace demiplane::db::postgres
