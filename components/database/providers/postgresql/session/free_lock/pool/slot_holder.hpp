#pragma once

#include <atomic>
#include <memory>

#include <connection_holder.hpp>

namespace demiplane::db::postgres {

    /**
     * Ring-buffer concrete ConnectionHolder for ConnectionPool.
     *
     * Stores the atomic slot status consumed by the pool's CAS acquire path.
     * Cache-line aligned to avoid false sharing. Pool owns each SlotHolder
     * through a std::shared_ptr; capabilities receive std::weak_ptr<ConnectionHolder>
     * derived from the pool's strong ref.
     */
    class alignas(64) SlotHolder final : public ConnectionHolder, public std::enable_shared_from_this<SlotHolder> {
    public:
        std::atomic<SlotStatus> status = SlotStatus::INACTIVE;

        SlotHolder() noexcept = default;
        explicit SlotHolder(PGconn* conn) noexcept
            : ConnectionHolder{conn} {
        }

        ~SlotHolder() override {
            if (conn_) {
                PQfinish(conn_);
                conn_ = nullptr;
            }
        }

        void reset() noexcept override {
            if (!run_cleanup_sql()) {
                status.store(SlotStatus::DEAD, std::memory_order_release);
                return;
            }
            status.store(SlotStatus::FREE, std::memory_order_release);
        }

        /// Replace the underlying PGconn (used by the janitor during replacement).
        void replace_conn(PGconn* new_conn) noexcept {
            if (conn_) {
                PQfinish(conn_);
            }
            conn_ = new_conn;
        }
    };

}  // namespace demiplane::db::postgres
