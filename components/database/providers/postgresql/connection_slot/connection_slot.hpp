#pragma once

#include <atomic>
#include <cstdint>
#include <string_view>

#include <gears_class_traits.hpp>
#include <libpq-fe.h>

namespace demiplane::db::postgres {

    /**
     * @brief Status of a connection slot in the cylinder ring buffer
     *
     * Transitions:
     *   INACTIVE --(cylinder init/grow)--> FREE
     *   FREE     --(acquire CAS)---------> USED
     *   USED     --(executor dtor/reset)--> FREE
     *   USED     --(async chain)---------> WAITING --(chain done)--> FREE
     *   USED     --(health fail)---------> DEAD
     *   WAITING  --(health fail)---------> DEAD
     *   DEAD     --(janitor)-------------> FREE (new PGconn*) or INACTIVE
     */
    enum class SlotStatus : std::uint8_t {
        FREE,      // Connection available for borrowing
        USED,      // Connection checked out to a capability
        WAITING,   // Connection in use, caller waiting on async chain
        DEAD,      // Connection lost or unresponsive, needs replacement
        INACTIVE,  // Slot intentionally empty (not yet initialized)
    };

    [[nodiscard]] constexpr const char* to_string(const SlotStatus status) noexcept {
        switch (status) {
            case SlotStatus::FREE:
                return "FREE";
            case SlotStatus::USED:
                return "USED";
            case SlotStatus::WAITING:
                return "WAITING";
            case SlotStatus::DEAD:
                return "DEAD";
            case SlotStatus::INACTIVE:
                return "INACTIVE";
        }
        return "UNKNOWN";
    }

    /**
     * @brief A single slot in the connection cylinder ring buffer
     *
     * Each slot is cache-line padded to prevent false sharing between
     * threads contending on adjacent slots during CAS acquire operations.
     *
     * Slots are self-contained: the executor destructor calls reset()
     * to run cleanup SQL and mark the slot FREE, eliminating the need
     * for a central release() method with O(n) slot search.
     */
    struct alignas(64) ConnectionSlot : gears::Immutable {
        PGconn* conn                   = nullptr;
        std::atomic<SlotStatus> status = SlotStatus::INACTIVE;
        std::string_view cleanup_sql{};  // set from CylinderConfig during init

        ConnectionSlot() noexcept = default;

        /**
         * @brief Self-contained reset: runs cleanup SQL, marks slot FREE
         *
         * Called by executor destructors. Safe to call on dead/finished
         * connections — checks PQstatus before issuing cleanup SQL.
         *
         * If the connection is unhealthy or cleanup SQL fails, marks the
         * slot DEAD so the janitor replaces it on the next sweep.
         */
        void reset() noexcept {
            if (!conn || PQstatus(conn) != CONNECTION_OK) {
                status.store(SlotStatus::DEAD, std::memory_order_release);
                return;
            }

            if (!cleanup_sql.empty()) {
                PGresult* res = PQexec(conn, cleanup_sql.data());
                const bool ok = res && PQresultStatus(res) == PGRES_COMMAND_OK;
                PQclear(res);

                if (!ok) {
                    status.store(SlotStatus::DEAD, std::memory_order_release);
                    return;
                }
            }

            status.store(SlotStatus::FREE, std::memory_order_release);
        }
    };

}  // namespace demiplane::db::postgres
