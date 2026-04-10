#include "connection_pool.hpp"

#include <postgres_errors.hpp>

namespace demiplane::db::postgres {

    ConnectionPool::ConnectionPool(ConnectionConfig connection_config, PoolConfig pool_config)
        : connection_config_{std::move(connection_config)},
          pool_config_{std::move(pool_config)},
          mask_{pool_config_.capacity() - 1},
          slots_(pool_config_.capacity()) {
        pool_config_.validate();

        // Eagerly create min_connections
        for (std::size_t i = 0; i < pool_config_.min_connections(); ++i) {
            slots_[i].conn = create_connection();
            if (slots_[i].conn) {
                slots_[i].status.store(SlotStatus::FREE, std::memory_order_release);
            } else {
                slots_[i].status.store(SlotStatus::DEAD, std::memory_order_release);
            }
        }
    }

    ConnectionPool::~ConnectionPool() {
        shutdown();  // ensure flag set

        // Unconditional termination: close ALL connections
        for (auto& slot : slots_) {
            if (slot.conn) {
                PQfinish(slot.conn);
                slot.conn = nullptr;
            }
        }
    }

    ConnectionSlot* ConnectionPool::acquire_slot() noexcept {
        if (shutdown_.load(std::memory_order_acquire)) {
            return nullptr;
        }

        const auto start = static_cast<std::size_t>(hint_cursor_.get_volatile());
        const auto cap   = pool_config_.capacity();

        // First pass: look for FREE slots
        for (std::size_t i = 0; i < cap; ++i) {
            const auto idx = (start + i) & mask_;
            if (auto expected = SlotStatus::FREE; slots_[idx].status.compare_exchange_strong(
                    expected, SlotStatus::USED, std::memory_order_acq_rel, std::memory_order_relaxed)) {
                hint_cursor_.set(static_cast<std::int64_t>((idx + 1) & mask_));
                return &slots_[idx];
            }
        }

        // Second pass: try to lazily init INACTIVE slots
        for (std::size_t i = 0; i < cap; ++i) {
            const auto idx = (start + i) & mask_;
            if (auto expected = SlotStatus::INACTIVE; slots_[idx].status.compare_exchange_strong(
                    expected, SlotStatus::USED, std::memory_order_acq_rel, std::memory_order_relaxed)) {
                slots_[idx].conn = create_connection();
                if (!slots_[idx].conn) {
                    slots_[idx].status.store(SlotStatus::DEAD, std::memory_order_release);
                    continue;
                }
                hint_cursor_.set(static_cast<std::int64_t>((idx + 1) & mask_));
                return &slots_[idx];
            }
        }

        return nullptr;  // Pool exhausted
    }

    void ConnectionPool::shutdown() {
        if (shutdown_.exchange(true, std::memory_order_acq_rel)) {
            return;  // Already shut down
        }

        // Graceful: close idle connections, skip borrowed ones
        for (std::size_t i = 0; i < pool_config_.capacity(); ++i) {
            if (const auto status = slots_[i].status.load(std::memory_order_acquire);
                status == SlotStatus::USED || status == SlotStatus::WAITING) {
                continue;  // Still borrowed — executor will call slot->reset()
            }
            if (slots_[i].conn) {
                PQfinish(slots_[i].conn);
                slots_[i].conn = nullptr;
            }
            slots_[i].status.store(SlotStatus::INACTIVE, std::memory_order_release);
        }
    }

    // ============== Stats ==============

    std::size_t ConnectionPool::capacity() const noexcept {
        return pool_config_.capacity();
    }

    std::size_t ConnectionPool::active_count() const noexcept {
        std::size_t count = 0;
        for (std::size_t i = 0; i < pool_config_.capacity(); ++i) {
            if (const auto s = slots_[i].status.load(std::memory_order_relaxed);
                s == SlotStatus::USED || s == SlotStatus::WAITING) {
                ++count;
            }
        }
        return count;
    }

    std::size_t ConnectionPool::free_count() const noexcept {
        std::size_t count = 0;
        for (std::size_t i = 0; i < pool_config_.capacity(); ++i) {
            if (slots_[i].status.load(std::memory_order_relaxed) == SlotStatus::FREE) {
                ++count;
            }
        }
        return count;
    }

    bool ConnectionPool::is_shutdown() const noexcept {
        return shutdown_.load(std::memory_order_acquire);
    }

    // ============== Internal Access ==============

    std::vector<ConnectionSlot>& ConnectionPool::slots() noexcept {
        return slots_;
    }

    const ConnectionConfig& ConnectionPool::connection_config() const noexcept {
        return connection_config_;
    }

    const PoolConfig& ConnectionPool::pool_config() const noexcept {
        return pool_config_;
    }

    PGconn* ConnectionPool::create_connection() const {
        const auto conn_string = connection_config_.to_connection_string();
        PGconn* conn           = PQconnectdb(conn_string.c_str());

        if (!conn || PQstatus(conn) != CONNECTION_OK) {
            if (conn) {
                PQfinish(conn);
            }
            return nullptr;
        }

        return conn;
    }

}  // namespace demiplane::db::postgres
