#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <memory>
#include <vector>

#include <connection_holder.hpp>
#include <gears_class_traits.hpp>
#include <pool_config.hpp>
#include <postgres_connection_config.hpp>
#include <sequence.hpp>

#include "slot_holder.hpp"

namespace demiplane::db::postgres {

    /**
     * Lock-free connection pool using a disruptor-inspired ring buffer.
     *
     * Holds a fixed-size vector of std::shared_ptr<SlotHolder>. Acquire uses
     * a CAS scan from a hint cursor for O(1) amortized borrowing and returns
     * a std::weak_ptr<ConnectionHolder> that capabilities keep as their
     * borrow handle. Reset is the capability's responsibility via the
     * SlotHolder's virtual reset().
     *
     * INACTIVE slots are lazily promoted to FREE on first acquire that
     * finds no FREE slot. The pool does not grow dynamically.
     */
    class ConnectionPool : gears::Immutable {
    public:
        ConnectionPool(ConnectionConfig connection_config, PoolConfig pool_config);
        ~ConnectionPool();

        /**
         * Acquire a connection holder from the pool (lock-free).
         * Returns a std::weak_ptr<ConnectionHolder>; expired() is true on exhaustion.
         */
        [[nodiscard]] std::weak_ptr<ConnectionHolder> acquire_slot() noexcept;

        /**
         * Acquire with a 10-step sleep-retry budget. Returns an expired weak on
         * full-budget failure. Deprecated entry point preserved for current Session
         * API compatibility.
         */
        [[nodiscard]] std::weak_ptr<ConnectionHolder>
        acquire_slot_wait(std::chrono::steady_clock::duration timeout) noexcept;

        /**
         * Graceful shutdown: mark INACTIVE all non-borrowed slots and prevent
         * further acquires. Borrowed slots are released by their capability dtor.
         */
        void shutdown();

        // ============== Stats ==============

        [[nodiscard]] std::size_t capacity() const noexcept;
        [[nodiscard]] std::size_t active_count() const noexcept;
        [[nodiscard]] std::size_t free_count() const noexcept;
        [[nodiscard]] bool is_shutdown() const noexcept;

        // ============== Internal Access (for Janitor) ==============

        [[nodiscard]] std::vector<std::shared_ptr<SlotHolder>>& slots() noexcept;
        [[nodiscard]] const ConnectionConfig& connection_config() const noexcept;
        [[nodiscard]] const PoolConfig& pool_config() const noexcept;

        /**
         * Create a new PGconn using the stored connection config.
         * Returns nullptr on failure.
         */
        [[nodiscard]] PGconn* create_connection() const;

    private:
        ConnectionConfig connection_config_;
        PoolConfig pool_config_;
        std::size_t mask_;

        std::vector<std::shared_ptr<SlotHolder>> slots_;
        multithread::Sequence hint_cursor_{0};

        std::atomic_bool shutdown_{false};
    };

}  // namespace demiplane::db::postgres
