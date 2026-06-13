#include "router.hpp"

#include <utility>

#include <errors.hpp>

namespace demiplane::http {

    boost::asio::awaitable<Response> Router::dispatch(RequestContext ctx) const {
        auto resolved = registry_->find_route(ctx.method(), ctx.path(), ctx.arena_alloc());
        if (!resolved) {
            co_return std::move(resolved).visit(
                [](ResolvedRoute&&) -> Response { std::unreachable(); },
                []<typename E>(E&& e) -> Response { return to_http_response(e); });
        }
        auto& [handler, path_params] = resolved.value();
        for (const auto& [name, value] : path_params)
            ctx.set_path_param(name, value);
        co_return co_await (*handler)(std::move(ctx));
    }

}  // namespace demiplane::http
