#pragma once

#include <concepts>
#include <functional>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <boost/asio/awaitable.hpp>
#include <demiplane/gears>
#include <demiplane/nexus>
#include <demiplane/scroll>

#include <async_outcome.hpp>
#include <errors.hpp>
#include <middleware.hpp>
#include <request_context.hpp>
#include <response.hpp>

namespace demiplane::http {

    class RouteRegistry;
    class HttpController;

    namespace detail {

        /// A typed error usable in a handler's Outcome: it must collapse to a
        /// Response via an ADL-found to_http_response(const E&). A missing
        /// overload fails this concept — the compile error names E (spec §8.3).
        template <typename E>
        concept HttpRenderableError = requires(const E& e) {
            { to_http_response(e) } -> std::same_as<Response>;
        };

        /// Maps a handler's return type onto the two supported shapes.
        template <typename R>
        struct RouteHandlerTraits {
            static constexpr bool valid = false;
        };
        template <>
        struct RouteHandlerTraits<boost::asio::awaitable<Response>> {
            static constexpr bool valid       = true;
            static constexpr bool has_outcome = false;
        };
        template <typename... Es>
        struct RouteHandlerTraits<boost::asio::awaitable<gears::Outcome<Response, Es...>>> {
            static constexpr bool valid       = (HttpRenderableError<Es> && ...);
            static constexpr bool has_outcome = true;
        };

        /// Collapse Outcome<Response, Es...> → Response via ADL (spec §8.3).
        /// The exact Response&& lambda beats the template in overload
        /// resolution, so errors land in the generic branch.
        template <typename OutcomeT>
        Response collapse_outcome(OutcomeT&& outcome) {
            return std::forward<OutcomeT>(outcome).visit(
                [](Response&& r) -> Response { return std::move(r); },
                []<typename E>(E&& e) -> Response { return to_http_response(e); });
        }

        /// Compose middlewares around `inner`, right-to-left, so the FIRST
        /// added middleware is the OUTERMOST (spec §8.3). Each layer is a
        /// plain (non-coroutine) lambda returning the middleware's awaitable
        /// directly — no wrapper frame; §11.1's budget stays at one frame per
        /// USER middleware. `next` lives in the layer closure inside the
        /// frozen chain, so the const& handed to the middleware (and held by
        /// its suspended frame) stays valid for the request's lifetime.
        ContextHandler wrap_with_middleware(ContextHandler inner,
                                            std::span<const Middleware> middlewares);

        /// The bake step (spec §8.1 phase 2): runs configure_routes() exactly
        /// once, then drains the controller's local routes into `registry`
        /// with `prefix` applied and the controller's middleware chain +
        /// Outcome collapse composed in. Called by GroupBinding (and by
        /// Server::add_controller in PR 5). Throws std::logic_error on a
        /// second bake of the same controller.
        struct ControllerBaker {
            static void bake_into(RouteRegistry& registry,
                                  const std::shared_ptr<HttpController>& ctrl,
                                  std::string_view prefix);
        };

    }  // namespace detail

    /// A callable route handler: invocable with a RequestContext (by value),
    /// returning AsyncResponse or AsyncOutcome<Response, Es...> where every E
    /// has an ADL to_http_response.
    template <typename F>
    concept RouteHandler =
        std::invocable<std::decay_t<F>&, RequestContext>
        && detail::RouteHandlerTraits<
            std::invoke_result_t<std::decay_t<F>&, RequestContext>>::valid;

    /**
     * @brief Application base class (spec §8.2). Subclasses register routes in
     *        configure_routes() via the protected verb DSL; GroupBinding bakes
     *        them into the server-wide registry with prefix + middleware +
     *        Outcome→Response conversion pre-composed.
     *
     * Lifecycle: construct → add_middleware()* → bake (via
     * GroupBinding::add_controller, which calls configure_routes() once) →
     * frozen. Registration or add_middleware after bake throws.
     */
    class HttpController : public std::enable_shared_from_this<HttpController> {
    public:
        NEXUS_REGISTER(nexus::Resettable);

        virtual ~HttpController() = default;

        /// Populate the local route table via the verb DSL. Called exactly
        /// once by the bake step — do not call it yourself.
        virtual void configure_routes() = 0;

        /// Lifecycle hooks; the Server wires them in PR 5.
        virtual void initialize() {
        }
        virtual void shutdown() {
        }

        /// Middlewares run in ADDITION ORDER (first added = outermost).
        template <typename Mw>
            requires std::constructible_from<Middleware, Mw&&>
        HttpController& add_middleware(Mw&& mw) {
            if (baked_)
                throw std::logic_error{"HttpController: add_middleware after bake"};
            middlewares_.push_back(Middleware{std::forward<Mw>(mw)});
            return *this;
        }

    protected:
        // ── Verb DSL: 3 shapes × 7 verbs (spec §8.2) ──────────────────────
        template <std::derived_from<HttpController> C>
        void Get(std::string path, AsyncResponse (C::*m)(RequestContext)) {
            member_route(HttpMethod::get, std::move(path), m);
        }
        template <std::derived_from<HttpController> C, typename... Es>
        void Get(std::string path, AsyncOutcome<Response, Es...> (C::*m)(RequestContext)) {
            member_outcome_route(HttpMethod::get, std::move(path), m);
        }
        template <RouteHandler F>
        void Get(std::string path, F&& f) {
            callable_route(HttpMethod::get, std::move(path), std::forward<F>(f));
        }

        template <std::derived_from<HttpController> C>
        void Post(std::string path, AsyncResponse (C::*m)(RequestContext)) {
            member_route(HttpMethod::post, std::move(path), m);
        }
        template <std::derived_from<HttpController> C, typename... Es>
        void Post(std::string path, AsyncOutcome<Response, Es...> (C::*m)(RequestContext)) {
            member_outcome_route(HttpMethod::post, std::move(path), m);
        }
        template <RouteHandler F>
        void Post(std::string path, F&& f) {
            callable_route(HttpMethod::post, std::move(path), std::forward<F>(f));
        }

        template <std::derived_from<HttpController> C>
        void Put(std::string path, AsyncResponse (C::*m)(RequestContext)) {
            member_route(HttpMethod::put, std::move(path), m);
        }
        template <std::derived_from<HttpController> C, typename... Es>
        void Put(std::string path, AsyncOutcome<Response, Es...> (C::*m)(RequestContext)) {
            member_outcome_route(HttpMethod::put, std::move(path), m);
        }
        template <RouteHandler F>
        void Put(std::string path, F&& f) {
            callable_route(HttpMethod::put, std::move(path), std::forward<F>(f));
        }

        template <std::derived_from<HttpController> C>
        void Patch(std::string path, AsyncResponse (C::*m)(RequestContext)) {
            member_route(HttpMethod::patch, std::move(path), m);
        }
        template <std::derived_from<HttpController> C, typename... Es>
        void Patch(std::string path, AsyncOutcome<Response, Es...> (C::*m)(RequestContext)) {
            member_outcome_route(HttpMethod::patch, std::move(path), m);
        }
        template <RouteHandler F>
        void Patch(std::string path, F&& f) {
            callable_route(HttpMethod::patch, std::move(path), std::forward<F>(f));
        }

        template <std::derived_from<HttpController> C>
        void Delete(std::string path, AsyncResponse (C::*m)(RequestContext)) {
            member_route(HttpMethod::del, std::move(path), m);
        }
        template <std::derived_from<HttpController> C, typename... Es>
        void Delete(std::string path, AsyncOutcome<Response, Es...> (C::*m)(RequestContext)) {
            member_outcome_route(HttpMethod::del, std::move(path), m);
        }
        template <RouteHandler F>
        void Delete(std::string path, F&& f) {
            callable_route(HttpMethod::del, std::move(path), std::forward<F>(f));
        }

        template <std::derived_from<HttpController> C>
        void Head(std::string path, AsyncResponse (C::*m)(RequestContext)) {
            member_route(HttpMethod::head, std::move(path), m);
        }
        template <std::derived_from<HttpController> C, typename... Es>
        void Head(std::string path, AsyncOutcome<Response, Es...> (C::*m)(RequestContext)) {
            member_outcome_route(HttpMethod::head, std::move(path), m);
        }
        template <RouteHandler F>
        void Head(std::string path, F&& f) {
            callable_route(HttpMethod::head, std::move(path), std::forward<F>(f));
        }

        template <std::derived_from<HttpController> C>
        void Options(std::string path, AsyncResponse (C::*m)(RequestContext)) {
            member_route(HttpMethod::options, std::move(path), m);
        }
        template <std::derived_from<HttpController> C, typename... Es>
        void Options(std::string path, AsyncOutcome<Response, Es...> (C::*m)(RequestContext)) {
            member_outcome_route(HttpMethod::options, std::move(path), m);
        }
        template <RouteHandler F>
        void Options(std::string path, F&& f) {
            callable_route(HttpMethod::options, std::move(path), std::forward<F>(f));
        }

    private:
        friend struct detail::ControllerBaker;

        using BakeFn = std::function<ContextHandler(const std::shared_ptr<HttpController>&,
                                                    std::span<const Middleware>)>;
        struct LocalRoute {
            HttpMethod method;
            std::string path;
            BakeFn bake;
        };

        void push_route(HttpMethod method, std::string path, BakeFn bake);

        template <std::derived_from<HttpController> C>
        static std::shared_ptr<C> typed_self(const std::shared_ptr<HttpController>& self) {
            // Bake-time only — never on the request path (spec §3 forbids
            // runtime dynamic_cast). Guards Get("/x", &OtherController::h)
            // cross-registration with a startup error instead of UB.
            auto typed = std::dynamic_pointer_cast<C>(self);
            if (!typed)
                throw std::logic_error{"HttpController: registered member function does not "
                                       "belong to the baked controller type"};
            return typed;
        }

        template <std::derived_from<HttpController> C>
        void member_route(const HttpMethod method, std::string path,
                          AsyncResponse (C::*m)(RequestContext)) {
            push_route(method, std::move(path),
                       [m](const std::shared_ptr<HttpController>& self,
                           const std::span<const Middleware> mws) -> ContextHandler {
                           // Plain lambda returning the member coroutine's
                           // awaitable directly — no wrapper frame. `typed`
                           // keeps the controller alive in the closure, which
                           // lives in the frozen registry.
                           ContextHandler inner = [typed = typed_self<C>(self),
                                                   m](RequestContext ctx) -> AsyncResponse {
                               return (typed.get()->*m)(std::move(ctx));
                           };
                           return detail::wrap_with_middleware(std::move(inner), mws);
                       });
        }

        template <std::derived_from<HttpController> C, typename... Es>
            requires(detail::HttpRenderableError<Es> && ...)
        void member_outcome_route(const HttpMethod method, std::string path,
                                  AsyncOutcome<Response, Es...> (C::*m)(RequestContext)) {
            push_route(method, std::move(path),
                       [m](const std::shared_ptr<HttpController>& self,
                           const std::span<const Middleware> mws) -> ContextHandler {
                           ContextHandler inner = [typed = typed_self<C>(self),
                                                   m](RequestContext ctx) -> AsyncResponse {
                               auto outcome = co_await (typed.get()->*m)(std::move(ctx));
                               co_return detail::collapse_outcome(std::move(outcome));
                           };
                           return detail::wrap_with_middleware(std::move(inner), mws);
                       });
        }

        template <RouteHandler F>
        void callable_route(const HttpMethod method, std::string path, F&& f) {
            using Fn     = std::decay_t<F>;
            using Traits = detail::RouteHandlerTraits<std::invoke_result_t<Fn&, RequestContext>>;
            push_route(
                method, std::move(path),
                [f = Fn{std::forward<F>(f)}](const std::shared_ptr<HttpController>&,
                                             const std::span<const Middleware> mws) mutable -> ContextHandler {
                    ContextHandler inner;
                    if constexpr (Traits::has_outcome) {
                        inner = [f](RequestContext ctx) mutable -> AsyncResponse {
                            auto outcome = co_await f(std::move(ctx));
                            co_return detail::collapse_outcome(std::move(outcome));
                        };
                    } else {
                        inner = ContextHandler{std::move(f)};  // signature matches exactly
                    }
                    return detail::wrap_with_middleware(std::move(inner), mws);
                });
        }

        bool configured_ = false;
        bool baked_      = false;
        std::vector<LocalRoute> local_routes_;
        std::vector<Middleware> middlewares_;

        SCROLL_COMPONENT_PREFIX("HttpController");
    };

}  // namespace demiplane::http
