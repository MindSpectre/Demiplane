#pragma once

#include <cstdint>

#include <libpq-fe.h>

namespace demiplane::db::postgres {

    // ============== CleanupQuery ==============

    enum class CleanupQuery : std::uint8_t {
        None,
        ResetAll,
        DeallocateAll,
        DiscardTemp,
        DiscardAll,
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

    // ============== SlotStatus ==============

    enum class SlotStatus : std::uint8_t {
        FREE,
        USED,
        WAITING,
        DEAD,
        INACTIVE,
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

    // ============== ConnectionHolder base class ==============

    /**
     * Polymorphic base class for pool-managed PostgreSQL connection handles.
     * Capabilities receive std::weak_ptr<ConnectionHolder>. On capability
     * destruction the weak_ptr is locked to a temporary shared_ptr and reset()
     * is invoked, which runs cleanup SQL and returns the connection via a
     * pool-specific release path.
     */
    class ConnectionHolder {
    public:
        virtual ~ConnectionHolder() = default;

        ConnectionHolder(const ConnectionHolder&)            = delete;
        ConnectionHolder& operator=(const ConnectionHolder&) = delete;
        ConnectionHolder(ConnectionHolder&&)                 = delete;
        ConnectionHolder& operator=(ConnectionHolder&&)      = delete;

        virtual void reset() noexcept = 0;

        [[nodiscard]] PGconn* conn() const noexcept {
            return conn_;
        }

        void set_cleanup(const CleanupQuery q) noexcept {
            cleanup_sql_ = to_sql(q);
        }

    protected:
        ConnectionHolder() noexcept = default;
        explicit ConnectionHolder(PGconn* conn) noexcept
            : conn_{conn} {
        }

        [[nodiscard]] bool run_cleanup_sql() noexcept {
            if (!conn_ || PQstatus(conn_) != CONNECTION_OK)
                return false;
            if (cleanup_sql_ == nullptr || cleanup_sql_[0] == '\0')
                return true;
            PGresult* res = PQexec(conn_, cleanup_sql_);
            const bool ok = res && PQresultStatus(res) == PGRES_COMMAND_OK;
            PQclear(res);
            cleanup_sql_ = nullptr;
            return ok;
        }

        PGconn* conn_            = nullptr;
        const char* cleanup_sql_ = nullptr;
    };

}  // namespace demiplane::db::postgres
