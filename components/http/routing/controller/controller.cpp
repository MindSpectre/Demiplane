#include "controller.hpp"

#include <ranges>

#include <route_registry.hpp>

namespace demiplane::http {

    void HttpController::push_route(const HttpMethod method, std::string path, BakeFn bake) {
        if (baked_)
            throw std::logic_error{"HttpController: route registration after bake"};
        local_routes_.push_back(LocalRoute{method, std::move(path), std::move(bake)});
    }

    namespace detail {

        ContextHandler wrap_with_middleware(ContextHandler inner,
                                            const std::span<const Middleware> middlewares) {
            for (const Middleware& mw : std::ranges::reverse_view(middlewares)) {
                NextHandler next = std::move(inner);
                // Plain lambda: returns the middleware coroutine's awaitable
                // directly (no wrapper frame). `mw` is copied once at BAKE
                // time; per request nothing is copied — `next` is handed down
                // by const& (spec §8.3).
                inner = [mw, next = std::move(next)](RequestContext ctx) -> AsyncResponse {
                    return mw(std::move(ctx), next);
                };
            }
            return inner;
        }

        void ControllerBaker::bake_into(RouteRegistry& registry,
                                        const std::shared_ptr<HttpController>& ctrl,
                                        const std::string_view prefix) {
            if (!ctrl)
                throw std::invalid_argument{"ControllerBaker: null controller"};
            if (ctrl->baked_)
                throw std::logic_error{"HttpController: controller already baked"};
            if (!ctrl->configured_) {
                ctrl->configure_routes();
                ctrl->configured_ = true;
            }
            ctrl->baked_ = true;  // mark baked before draining: a failed bake is not retryable
            for (auto& [method, path, bake] : ctrl->local_routes_) {
                registry.add_route(
                    method, join_path(prefix, path), bake(ctrl, std::span<const Middleware>{ctrl->middlewares_}));
            }
            ctrl->local_routes_.clear();  // baked closures live in the registry now
        }

    }  // namespace detail

}  // namespace demiplane::http
