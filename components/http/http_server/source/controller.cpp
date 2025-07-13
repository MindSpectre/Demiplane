#include "controller.hpp"

namespace demiplane::http {

    void HttpController::transfer_routes_to(RouteRegistry& target_registry) {
        target_registry.merge(std::move(registry_));
    }

    size_t HttpController::route_count() const {
        return registry_.route_count();
    }

} // namespace demiplane::http
