#include "router.hpp"

#include <utility>

#include <errors.hpp>

namespace demiplane::http {

    namespace {
        // Cold path only (routing misses): an asio::awaitable cannot be
        // constructed from a ready value, so 404/405 pay one tiny frame.
        AsyncResponse ready_response(Response r) {
            co_return r;
        }
    }  // namespace

    AsyncResponse Router::dispatch(RequestContext ctx) const {
        // [unlikely]: the hookless fast path is THE design target (zero-alloc,
        // no dispatch frame); hook users enter a full coroutine anyway.
        if (hooks_.on_request || hooks_.on_response || hooks_.on_unhandled_exception) [[unlikely]]
            return dispatch_with_hooks(std::move(ctx));

        auto resolved = registry_->find_route(ctx.method(), ctx.path(), ctx.arena_alloc());
        if (!resolved) [[unlikely]]
            return ready_response(
                std::move(resolved).visit([](ResolvedRoute&&) -> Response { std::unreachable(); },
                                          []<typename E>(E&& e) -> Response { return to_http_response(e); }));
        auto& [handler, path_params] = resolved.value();
        for (const auto& [name, value] : path_params)
            ctx.set_path_param(name, value);
        // The handler's coroutine frame is created and `ctx` moved into it AT
        // CALL TIME; returning the un-awaited awaitable is the established
        // pattern (controller.hpp member_route). A synchronous throw escapes
        // dispatch() itself — same driver catch-all either way.
        return (*handler)(std::move(ctx));
    }

    AsyncResponse Router::dispatch_with_hooks(RequestContext ctx) const {
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
