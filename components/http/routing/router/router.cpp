#include "router.hpp"

#include <utility>

#include <errors.hpp>

namespace demiplane::http {
    boost::asio::awaitable<Response> Router::dispatch(RequestContext ctx) const {
        if (hooks_.on_request)
            hooks_.on_request(ctx);
        const RequestInfo info{ctx.method(), ctx.target()};

        auto resolved = registry_->find_route(ctx.method(), ctx.path(), ctx.arena_alloc());
        if (!resolved) {
            Response r = std::move(resolved).visit([](ResolvedRoute&&) -> Response { std::unreachable(); },
                                                   []<typename E>(E&& e) -> Response { return to_http_response(e); });
            if (hooks_.on_response)
                hooks_.on_response(info, r);
            co_return r;
        }
        auto& [handler, path_params] = resolved.value();
        for (const auto& [name, value] : path_params)
            ctx.set_path_param(name, value);
        try {
            Response r = co_await (*handler)(std::move(ctx));
            if (hooks_.on_response)
                hooks_.on_response(info, r);
            co_return r;
        } catch (...) {
            if (hooks_.on_unhandled_exception)
                hooks_.on_unhandled_exception(std::current_exception());
            throw;  // driver catch-all converts to 500 (spec §6.3)
        }
    }

}  // namespace demiplane::http
