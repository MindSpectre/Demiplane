#include "cylinder_connection.hpp"

#include <postgres_errors.hpp>

namespace demiplane::db::postgres {

    ConnectionCylinder::ConnectionCylinder(ConnectionConfig connection_config, CylinderConfig cylinder_config)
        : connection_config_{std::move(connection_config)},
          cylinder_config_{std::move(cylinder_config)},
          mask_{cylinder_config_.capacity() - 1},
          slots_(cylinder_config_.capacity()) {

        cylinder_config_.validate();

        // Set cleanup_sql on each slot (string_view into config's string)
        for (auto& slot : slots_) {
            slot.cleanup_sql = cylinder_config_.cleanup_sql();
        }

        // Eagerly create min_connections
        for (std::size_t i = 0; i < cylinder_config_.min_connections(); ++i) {
            slots_[i].conn = create_connection();
            if (slots_[i].conn) {
                slots_[i].status.store(SlotStatus::FREE, std::memory_order_release);
            } else {
                slots_[i].status.store(SlotStatus::DEAD, std::memory_order_release);
            }
        }
    }

    ConnectionCylinder::~ConnectionCylinder() {
        shutdown();  // ensure flag set

        // Unconditional termination: close ALL connections
        for (auto& slot : slots_) {
            if (slot.conn) {
                PQfinish(slot.conn);
                slot.conn = nullptr;
            }
        }
    }

    ConnectionSlot* ConnectionCylinder::acquire_slot() noexcept {
        if (shutdown_.load(std::memory_order_acquire)) {
            return nullptr;
        }

        const auto start = static_cast<std::size_t>(hint_cursor_.get_volatile());
        const auto cap   = cylinder_config_.capacity();

        // First pass: look for FREE slots
        for (std::size_t i = 0; i < cap; ++i) {
            const auto idx = (start + i) & mask_;
            auto expected  = SlotStatus::FREE;
            if (slots_[idx].status.compare_exchange_strong(
                    expected, SlotStatus::USED, std::memory_order_acq_rel, std::memory_order_relaxed)) {
                hint_cursor_.set(static_cast<std::int64_t>((idx + 1) & mask_));
                return &slots_[idx];
            }
        }

        // Second pass: try to lazily init INACTIVE slots
        for (std::size_t i = 0; i < cap; ++i) {
            const auto idx = (start + i) & mask_;
            auto expected  = SlotStatus::INACTIVE;
            if (slots_[idx].status.compare_exchange_strong(
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

        return nullptr;  // Cylinder exhausted
    }

    void ConnectionCylinder::shutdown() {
        if (shutdown_.exchange(true, std::memory_order_acq_rel)) {
            return;  // Already shut down
        }

        // Graceful: close idle connections, skip borrowed ones
        for (std::size_t i = 0; i < cylinder_config_.capacity(); ++i) {
            const auto status = slots_[i].status.load(std::memory_order_acquire);
            if (status == SlotStatus::USED || status == SlotStatus::WAITING) {
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

    std::size_t ConnectionCylinder::capacity() const noexcept {
        return cylinder_config_.capacity();
    }

    std::size_t ConnectionCylinder::active_count() const noexcept {
        std::size_t count = 0;
        for (std::size_t i = 0; i < cylinder_config_.capacity(); ++i) {
            const auto s = slots_[i].status.load(std::memory_order_relaxed);
            if (s == SlotStatus::USED || s == SlotStatus::WAITING) {
                ++count;
            }
        }
        return count;
    }

    std::size_t ConnectionCylinder::free_count() const noexcept {
        std::size_t count = 0;
        for (std::size_t i = 0; i < cylinder_config_.capacity(); ++i) {
            if (slots_[i].status.load(std::memory_order_relaxed) == SlotStatus::FREE) {
                ++count;
            }
        }
        return count;
    }

    bool ConnectionCylinder::is_shutdown() const noexcept {
        return shutdown_.load(std::memory_order_acquire);
    }

    // ============== Internal Access ==============

    std::vector<ConnectionSlot>& ConnectionCylinder::slots() noexcept {
        return slots_;
    }

    const ConnectionConfig& ConnectionCylinder::connection_config() const noexcept {
        return connection_config_;
    }

    const CylinderConfig& ConnectionCylinder::cylinder_config() const noexcept {
        return cylinder_config_;
    }

    PGconn* ConnectionCylinder::create_connection() const {
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
