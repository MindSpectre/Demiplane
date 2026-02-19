#pragma once

#include <atomic>
#include <cstddef>
#include <vector>

#include <postgres_connection_config.hpp>
#include <connection_slot.hpp>
#include <gears_class_traits.hpp>
#include <sequence.hpp>

#include "../config/cylinder_config.hpp"

namespace demiplane::db::postgres {

    /**
     * @brief Lock-free connection cylinder using a disruptor-inspired ring buffer
     *
     * Manages a fixed-size ring buffer of PGconn* connections. Acquire uses
     * a CAS scan from a hint cursor for O(1) amortized borrowing. Slot reset
     * is the executor's responsibility via ConnectionSlot::reset().
     *
     * INACTIVE slots are lazily promoted to FREE on first acquire that
     * finds no FREE slot. The cylinder does not grow dynamically.
     */
    class ConnectionCylinder : gears::Immutable {
    public:
        ConnectionCylinder(ConnectionConfig connection_config, CylinderConfig cylinder_config);
        ~ConnectionCylinder();

        /**
         * @brief Acquire a connection slot from the cylinder (lock-free)
         * @return ConnectionSlot* on success, nullptr if cylinder exhausted
         *
         * Scans from hint_cursor_ using CAS on FREE slots.
         * Lazily initializes INACTIVE slots when no FREE slot is found.
         */
        [[nodiscard]] ConnectionSlot* acquire_slot() noexcept;

        /**
         * @brief Graceful shutdown: close idle connections, prevent new acquisitions
         *
         * Skips USED and WAITING slots — those are still borrowed by executors.
         * Executor destructors will call slot->reset() when they're done.
         */
        void shutdown();

        // ============== Stats ==============

        [[nodiscard]] std::size_t capacity() const noexcept;
        [[nodiscard]] std::size_t active_count() const noexcept;
        [[nodiscard]] std::size_t free_count() const noexcept;
        [[nodiscard]] bool is_shutdown() const noexcept;

        // ============== Internal Access (for Janitor) ==============

        [[nodiscard]] std::vector<ConnectionSlot>& slots() noexcept;
        [[nodiscard]] const ConnectionConfig& connection_config() const noexcept;
        [[nodiscard]] const CylinderConfig& cylinder_config() const noexcept;

        /**
         * @brief Create a new PGconn* using the stored connection config
         * @return New connection, or nullptr on failure
         */
        [[nodiscard]] PGconn* create_connection() const;

    private:
        ConnectionConfig connection_config_;
        CylinderConfig cylinder_config_;
        std::size_t mask_;

        std::vector<ConnectionSlot> slots_;
        multithread::Sequence hint_cursor_{0};

        std::atomic_bool shutdown_{false};
    };

}  // namespace demiplane::db::postgres
