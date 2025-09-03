#include "controller.hpp"

#include <demiplane/scroll>

namespace demiplane::http {
    void HttpController::transfer_routes_to(RouteRegistry& target_registry) {
        COMPONENT_LOG_INF() << "Transferring routes to target registry";
        target_registry.merge(std::move(registry_));
        COMPONENT_LOG_DBG() << "Routes transferred. Size is " << target_registry.route_count();
    }

    size_t HttpController::route_count() const {
        return registry_.route_count();
    }
}  // namespace demiplane::http
