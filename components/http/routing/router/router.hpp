#pragma once

#include <exception>
#include <functional>
#include <utility>

#include <async_outcome.hpp>
#include <boost/asio/awaitable.hpp>
#include <http_enums.hpp>
#include <request_context.hpp>
#include <response.hpp>
#include <route_registry.hpp>

namespace demiplane::http {

    /// Request identity snapshotted at dispatch entry for the on_response hook
    /// (D3): the RequestContext is CONSUMED by value by the handler chain, so
    /// it no longer exists when the response is available. `target` views
    /// connection-owned storage — valid through the hook call and the response
    /// write; the arena/buffer reset only at the next keep-alive iteration.
    struct RequestInfo {
        HttpMethod method{};
        std::string_view target;
    };

    /**
     * @brief Thin dispatch facade the protocol drivers call (spec §8.8).
     *
     * find_route + path-param injection + handler invocation. Routing misses
     * (404/405) and handler typed errors are already collapsed to Response by
     * the time dispatch returns; exceptions escape to the driver's catch-all
     * (PR 3). The registry must be frozen before the first dispatch; frozen
     * means immutable, so concurrent dispatch from N io threads is safe.
     *
     * Request-observation hooks (PR 5, D2): the Server wires fan-out lambdas
     * over its ServerObserver list at setup() — plain std::functions, so the
     * routing layer stays free of any server-layer dependency. When ALL hooks
     * are unset (no observers — the common case), dispatch is not a coroutine
     * at all: it resolves the route synchronously and tail-forwards the
     * handler's awaitable, so requests skip the dispatch frame entirely.
     * Hook contract:
     *  - on_request(ctx)        — dispatch entry, before routing;
     *  - on_response(info, r)   — handler successes AND routing-miss 404/405;
     *    NOT fired when the handler throws (the 500 is driver-synthesized);
     *  - on_unhandled_exception — handler escape; fired, then RETHROWN so the
     *    driver's catch-all still writes the 500.
     * Driver-level early responses (malformed 400, header/body-limit 4xx)
     * never reach the Router and are not observed.
     */
    class Router {
    public:
        struct Hooks {
            std::function<void(const RequestContext&)> on_request;
            std::function<void(const RequestInfo&, const Response&)> on_response;
            std::function<void(std::exception_ptr)> on_unhandled_exception;
        };

        explicit Router(const RouteRegistry& registry) noexcept
            : registry_{&registry} {
        }

        /// Build phase ONLY (single-threaded, before the accept loops spawn) —
        /// dispatch reads hooks_ unsynchronized from N io threads afterwards.
        void set_hooks(Hooks hooks) {
            hooks_ = std::move(hooks);
        }

        [[nodiscard]] AsyncResponse dispatch(RequestContext ctx) const;

    private:
        /// The pre-fast-path dispatch body, verbatim: hook calls around
        /// routing + handler await. Only taken when at least one hook is set.
        [[nodiscard]] AsyncResponse dispatch_with_hooks(RequestContext ctx) const;

        const RouteRegistry* registry_;
        Hooks hooks_;
    };

}  // namespace demiplane::http
