#pragma once

#include <functional>
#include <utility>

#include <async_outcome.hpp>
#include <request_context.hpp>

namespace demiplane::http {

    /// The baked, ready-to-call form of one route: prefix applied, middleware
    /// chain composed, Outcome→Response collapse wired (spec §8.3). Stored in
    /// the frozen RouteRegistry; the Router invokes it through a pointer —
    /// never a copy (copying a std::function may heap-allocate, spec §8.5).
    using ContextHandler = std::function<AsyncResponse(RequestContext)>;

    /// What a middleware calls to continue the chain. Passed by const& — NEVER
    /// by value: a by-value copy of the composed std::function would
    /// heap-allocate per layer per request (spec §8.3). The referent lives in
    /// the frozen baked chain and outlives any request, including the
    /// reference held by a middleware's suspended coroutine frame.
    using NextHandler = ContextHandler;

    /// A middleware can short-circuit (co_return without calling next),
    /// enrich the context (ctx.set<T>(...) then call next), or post-process
    /// (auto r = co_await next(std::move(ctx)); ...; co_return r;).
    using Middleware = std::function<AsyncResponse(RequestContext, const NextHandler&)>;

    /// Append several middlewares to a controller in one call (spec §8.4):
    ///     add_basic_middleware(*users, log_mw, request_id_mw, tracer_mw);
    /// They run in argument order (first = outermost).
    template <typename Controller, typename... Mws>
    void add_basic_middleware(Controller& controller, Mws&&... mws) {
        (controller.add_middleware(std::forward<Mws>(mws)), ...);
    }

}  // namespace demiplane::http
