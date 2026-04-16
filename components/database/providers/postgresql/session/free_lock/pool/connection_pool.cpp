#include "connection_pool.hpp"

#include <thread>

#include <postgres_errors.hpp>

namespace demiplane::db::postgres {

    ConnectionPool::ConnectionPool(ConnectionConfig connection_config, PoolConfig pool_config)
        : connection_config_{std::move(connection_config)},
          pool_config_{std::move(pool_config)},
          mask_{pool_config_.capacity() - 1},
          slots_(pool_config_.capacity()) {
        for (auto& slot : slots_) {
            slot = std::make_shared<SlotHolder>();
        }
        for (std::size_t i = 0; i < pool_config_.min_connections(); ++i) {
            if (PGconn* c = create_connection()) {
                slots_[i]->replace_conn(c);
                slots_[i]->status.store(SlotStatus::FREE, std::memory_order_release);
            } else {
                slots_[i]->status.store(SlotStatus::DEAD, std::memory_order_release);
            }
        }
    }

    ConnectionPool::~ConnectionPool() {
        shutdown();
        // slots_ destruction drops the canonical strong refs; each ~SlotHolder
        // closes its own PGconn via PQfinish.
    }

    std::weak_ptr<ConnectionHolder> ConnectionPool::acquire_slot() noexcept {
        if (shutdown_.load(std::memory_order_acquire))
            return {};

        const auto start = static_cast<std::size_t>(hint_cursor_.get_volatile());
        const auto cap   = pool_config_.capacity();

        for (std::size_t i = 0; i < cap; ++i) {
            const auto idx = (start + i) & mask_;
            auto& slot     = *slots_[idx];
            auto expected  = SlotStatus::FREE;
            if (slot.status.compare_exchange_strong(
                    expected, SlotStatus::USED, std::memory_order_acq_rel, std::memory_order_relaxed)) {
                hint_cursor_.set(static_cast<std::int64_t>((idx + 1) & mask_));
                return slots_[idx];
            }
        }

        for (std::size_t i = 0; i < cap; ++i) {
            const auto idx = (start + i) & mask_;
            auto& slot     = *slots_[idx];
            auto expected  = SlotStatus::INACTIVE;
            if (slot.status.compare_exchange_strong(
                    expected, SlotStatus::USED, std::memory_order_acq_rel, std::memory_order_relaxed)) {
                PGconn* fresh = create_connection();
                if (!fresh) {
                    slot.status.store(SlotStatus::DEAD, std::memory_order_release);
                    continue;
                }
                slot.replace_conn(fresh);
                hint_cursor_.set(static_cast<std::int64_t>((idx + 1) & mask_));
                return slots_[idx];
            }
        }

        return {};
    }

    std::weak_ptr<ConnectionHolder>
    ConnectionPool::acquire_slot_wait(std::chrono::steady_clock::duration timeout) noexcept {
        if (auto w = acquire_slot(); !w.expired()) {
            return w;
        }

        if (timeout <= std::chrono::steady_clock::duration::zero()) {
            return {};
        }

        const auto interval = timeout / 10;
        for (int attempt = 0; attempt < 10; ++attempt) {
            if (shutdown_.load(std::memory_order_acquire)) {
                return {};
            }
            std::this_thread::sleep_for(interval);
            if (auto w = acquire_slot(); !w.expired()) {
                return w;
            }
        }
        return {};
    }

    void ConnectionPool::shutdown() {
        if (shutdown_.exchange(true, std::memory_order_acq_rel)) {
            return;
        }

        for (auto& slot : slots_) {
            const auto s = slot->status.load(std::memory_order_acquire);
            if (s == SlotStatus::USED || s == SlotStatus::WAITING)
                continue;
            slot->status.store(SlotStatus::INACTIVE, std::memory_order_release);
        }
        // PGconn cleanup happens in ~SlotHolder when slots_ is destroyed.
    }

    // ============== Stats ==============

    std::size_t ConnectionPool::capacity() const noexcept {
        return pool_config_.capacity();
    }

    std::size_t ConnectionPool::active_count() const noexcept {
        std::size_t count = 0;
        for (std::size_t i = 0; i < pool_config_.capacity(); ++i) {
            if (const auto s = slots_[i]->status.load(std::memory_order_relaxed);
                s == SlotStatus::USED || s == SlotStatus::WAITING) {
                ++count;
            }
        }
        return count;
    }

    std::size_t ConnectionPool::free_count() const noexcept {
        std::size_t count = 0;
        for (std::size_t i = 0; i < pool_config_.capacity(); ++i) {
            if (slots_[i]->status.load(std::memory_order_relaxed) == SlotStatus::FREE) {
                ++count;
            }
        }
        return count;
    }

    bool ConnectionPool::is_shutdown() const noexcept {
        return shutdown_.load(std::memory_order_acquire);
    }

    // ============== Internal Access ==============

    std::vector<std::shared_ptr<SlotHolder>>& ConnectionPool::slots() noexcept {
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
