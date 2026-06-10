#pragma once

#include <boost/asio/awaitable.hpp>

#include <request_context.hpp>
#include <response.hpp>
#include <route_registry.hpp>

namespace demiplane::http {

    /**
     * @brief Thin dispatch facade the protocol drivers call (spec §8.8).
     *
     * find_route + path-param injection + handler invocation. Routing misses
     * (404/405) and handler typed errors are already collapsed to Response by
     * the time dispatch returns; exceptions escape to the driver's catch-all
     * (PR 3). The registry must be frozen before the first dispatch; frozen
     * means immutable, so concurrent dispatch from N io threads is safe.
     */
    class Router {
    public:
        explicit Router(const RouteRegistry& registry) noexcept
            : registry_{&registry} {
        }

        [[nodiscard]] boost::asio::awaitable<Response> dispatch(RequestContext ctx) const;

    private:
        const RouteRegistry* registry_;
    };

}  // namespace demiplane::http
