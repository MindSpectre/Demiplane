#pragma once

#include <thread>

#include "connection_pool.hpp"

namespace demiplane::db::postgres {

    /**
     * @brief Background health monitor for the connection pool
     *
     * Periodically sweeps the ring buffer to:
     *   - Replace DEAD connections with fresh ones
     *   - Check health of FREE connections with expired max_lifetime
     *   - Detect and mark unhealthy connections
     *
     * Uses std::jthread with stop_token for immediate cancellation
     * without spinning or sleeping through the full interval.
     */
    class PoolJanitor : gears::Immutable {
    public:
        explicit PoolJanitor(ConnectionPool& pool);
        ~PoolJanitor();

        void stop();

    private:
        void run(const std::stop_token& token) const;
        void sweep(const std::stop_token& token) const;

        ConnectionPool& pool_;
        std::jthread thread_;
    };

}  // namespace demiplane::db::postgres
