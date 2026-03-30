#pragma once

#include <thread>

#include "../connection/cylinder_connection.hpp"

namespace demiplane::db::postgres {

    /**
     * @brief Background health monitor for the connection cylinder
     *
     * Periodically sweeps the ring buffer to:
     *   - Replace DEAD connections with fresh ones
     *   - Check health of FREE connections with expired max_lifetime
     *   - Detect and mark unhealthy connections
     *
     * Uses std::jthread with stop_token for immediate cancellation
     * without spinning or sleeping through the full interval.
     */
    class CylinderJanitor : gears::Immutable {
    public:
        explicit CylinderJanitor(ConnectionCylinder& cylinder);
        ~CylinderJanitor();

        void stop();

    private:
        void run(const std::stop_token& token) const;
        void sweep(const std::stop_token& token) const;

        ConnectionCylinder& cylinder_;
        std::jthread thread_;
    };

}  // namespace demiplane::db::postgres
