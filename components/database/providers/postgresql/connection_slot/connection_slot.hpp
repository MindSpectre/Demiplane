#pragma once

#include <atomic>

#include <gears_class_traits.hpp>
#include <libpq-fe.h>

namespace demiplane::db::postgres {

    /**
     * @brief Predefined cleanup SQL statements for connection reset
     *
     * Each variant maps to a PostgreSQL session-reset command.
     * Use with do_cleanup() on executors/transactions to declare
     * what cleanup the slot needs when returned to the pool.
     */
    enum class CleanupQuery : std::uint8_t {
        None,           // No cleanup (stateless reads)
        ResetAll,       // "RESET ALL" — GUC parameters only
        DeallocateAll,  // "DEALLOCATE ALL" — prepared statements only
        DiscardTemp,    // "DISCARD TEMP" — temp tables/sequences
        DiscardAll,     // "DISCARD ALL" — full session reset (most expensive)
    };

    [[nodiscard]] constexpr const char* to_sql(const CleanupQuery query) noexcept {
        switch (query) {
            case CleanupQuery::None:
                return nullptr;
            case CleanupQuery::ResetAll:
                return "RESET ALL";
            case CleanupQuery::DeallocateAll:
                return "DEALLOCATE ALL";
            case CleanupQuery::DiscardTemp:
                return "DISCARD TEMP";
            case CleanupQuery::DiscardAll:
                return "DISCARD ALL";
        }
        return nullptr;
    }

    /**
     * @brief Status of a connection slot in the pool ring buffer
     *
     * Transitions:
     *   INACTIVE --(pool init/grow)--> FREE
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
     * @brief A single slot in the connection pool ring buffer
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
        const char* cleanup_sql        = nullptr;  // set from PoolConfig during init

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

            if (cleanup_sql != nullptr && cleanup_sql[0] != '\0') {
                PGresult* res = PQexec(conn, cleanup_sql);
                const bool ok = res && PQresultStatus(res) == PGRES_COMMAND_OK;
                PQclear(res);
                cleanup_sql = nullptr;  // purge — next borrower sets its own via do_cleanup()

                if (!ok) {
                    status.store(SlotStatus::DEAD, std::memory_order_release);
                    return;
                }
            }

            status.store(SlotStatus::FREE, std::memory_order_release);
        }
    };

}  // namespace demiplane::db::postgres
