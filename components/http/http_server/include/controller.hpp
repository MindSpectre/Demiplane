#pragma once

#include <memory>

#include "aliases.hpp"
#include "gears_utils.hpp"
#include "request_context.hpp"
#include "route_registry.hpp"

namespace demiplane::http {
    class HttpController;

    template <typename Controller>
    concept IsController = std::is_base_of_v<HttpController, Controller>;

    class HttpController : public std::enable_shared_from_this<HttpController> {
    public:
        virtual ~HttpController()       = default;
        virtual void configure_routes() = 0;
        virtual void initialize() {}
        virtual void shutdown() {}

        // Transfer routes to server registry - no longer expose internal registry
        void transfer_routes_to(RouteRegistry& target_registry);
        size_t route_count() const;

    protected:
        // Member function binding - RequestContext by value
        template <IsController Controller>
        void Get(std::string path, Response (Controller::*method)(RequestContext)) {
            registry_.add_route(boost::beast::http::verb::get, std::move(path), bind_sync_method(method));
        }

        template <IsController Controller>
        void Post(std::string path, Response (Controller::*method)(RequestContext)) {
            registry_.add_route(boost::beast::http::verb::post, std::move(path), bind_sync_method(method));
        }

        template <IsController Controller>
        void Put(std::string path, Response (Controller::*method)(RequestContext)) {
            registry_.add_route(boost::beast::http::verb::put, std::move(path), bind_sync_method(method));
        }

        template <IsController Controller>
        void Delete(std::string path, Response (Controller::*method)(RequestContext)) {
            registry_.add_route(boost::beast::http::verb::delete_, std::move(path), bind_sync_method(method));
        }

        // Member function binding - RequestContext by const reference
        template <IsController Controller>
        void Get(std::string path, Response (Controller::*method)(const RequestContext&)) {
            registry_.add_route(boost::beast::http::verb::get, std::move(path), bind_sync_method_const_ref(method));
        }

        template <IsController Controller>
        void Post(std::string path, Response (Controller::*method)(const RequestContext&)) {
            registry_.add_route(boost::beast::http::verb::post, std::move(path), bind_sync_method_const_ref(method));
        }

        template <IsController Controller>
        void Put(std::string path, Response (Controller::*method)(const RequestContext&)) {
            registry_.add_route(boost::beast::http::verb::put, std::move(path), bind_sync_method_const_ref(method));
        }

        template <IsController Controller>
        void Delete(std::string path, Response (Controller::*method)(const RequestContext&)) {
            registry_.add_route(boost::beast::http::verb::delete_, std::move(path), bind_sync_method_const_ref(method));
        }

        // Async method binding - RequestContext by value
        template <IsController Controller>
        void Get(std::string path, AsyncResponse (Controller::*method)(RequestContext)) {
            registry_.add_route(boost::beast::http::verb::get, std::move(path), bind_async_method(method));
        }

        template <IsController Controller>
        void Post(std::string path, AsyncResponse (Controller::*method)(RequestContext)) {
            registry_.add_route(boost::beast::http::verb::post, std::move(path), bind_async_method(method));
        }

        template <IsController Controller>
        void Put(std::string path, AsyncResponse (Controller::*method)(RequestContext)) {
            registry_.add_route(boost::beast::http::verb::put, std::move(path), bind_async_method(method));
        }

        template <IsController Controller>
        void Delete(std::string path, AsyncResponse (Controller::*method)(RequestContext)) {
            registry_.add_route(boost::beast::http::verb::delete_, std::move(path), bind_async_method(method));
        }

        // Async method binding - RequestContext by const reference
        template <IsController Controller>
        void Get(std::string path, AsyncResponse (Controller::*method)(const RequestContext&)) {
            registry_.add_route(boost::beast::http::verb::get, std::move(path), bind_async_method_const_ref(method));
        }

        template <IsController Controller>
        void Post(std::string path, AsyncResponse (Controller::*method)(const RequestContext&)) {
            registry_.add_route(boost::beast::http::verb::post, std::move(path), bind_async_method_const_ref(method));
        }

        template <IsController Controller>
        void Put(std::string path, AsyncResponse (Controller::*method)(const RequestContext&)) {
            registry_.add_route(boost::beast::http::verb::put, std::move(path), bind_async_method_const_ref(method));
        }

        template <IsController Controller>
        void Delete(std::string path, AsyncResponse (Controller::*method)(const RequestContext&)) {
            registry_.add_route(
                boost::beast::http::verb::delete_, std::move(path), bind_async_method_const_ref(method));
        }

        // Free function/callable binding with perfect forwarding
        template <typename F>
            requires (!std::is_member_function_pointer_v<std::decay_t<F>>)
        void Get(std::string path, F&& handler) {
            registry_.add_route(boost::beast::http::verb::get, std::move(path), wrap_handler(std::forward<F>(handler)));
        }

        template <typename F>
            requires (!std::is_member_function_pointer_v<std::decay_t<F>>)
        void Post(std::string path, F&& handler) {
            registry_.add_route(
                boost::beast::http::verb::post, std::move(path), wrap_handler(std::forward<F>(handler)));
        }

        template <typename F>
            requires (!std::is_member_function_pointer_v<std::decay_t<F>>)
        void Put(std::string path, F&& handler) {
            registry_.add_route(boost::beast::http::verb::put, std::move(path), wrap_handler(std::forward<F>(handler)));
        }

        template <typename F>
            requires (!std::is_member_function_pointer_v<std::decay_t<F>>)
        void Delete(std::string path, F&& handler) {
            registry_.add_route(
                boost::beast::http::verb::delete_, std::move(path), wrap_handler(std::forward<F>(handler)));
        }

    private:
        RouteRegistry registry_;

        // Binding helpers for member functions
        template <IsController Controller>
        ContextHandler bind_sync_method(Response (Controller::*method)(RequestContext)) {
            auto self = std::static_pointer_cast<Controller>(shared_from_this());
            return [self, method](
                       RequestContext ctx) -> AsyncResponse { co_return (self.get()->*method)(std::move(ctx)); };
        }

        template <IsController Controller>
        ContextHandler bind_sync_method_const_ref(Response (Controller::*method)(const RequestContext&)) {
            auto self = std::static_pointer_cast<Controller>(shared_from_this());
            return [self, method](RequestContext ctx) -> AsyncResponse { co_return (self.get()->*method)(ctx); };
        }

        template <IsController Controller>
        ContextHandler bind_async_method(AsyncResponse (Controller::*method)(RequestContext)) {
            auto self = std::static_pointer_cast<Controller>(shared_from_this());
            return [self, method](RequestContext ctx) -> AsyncResponse {
                co_return co_await (self.get()->*method)(std::move(ctx));
            };
        }

        template <IsController Controller>
        ContextHandler bind_async_method_const_ref(AsyncResponse (Controller::*method)(const RequestContext&)) {
            auto self = std::static_pointer_cast<Controller>(shared_from_this());
            return
                [self, method](RequestContext ctx) -> AsyncResponse { co_return co_await (self.get()->*method)(ctx); };
        }

        // Generic wrapper for free functions and lambdas
        template <typename F>
        ContextHandler wrap_handler(F&& handler) {
            if constexpr (std::is_invocable_r_v<Response, F, RequestContext>) {
                // Sync handler taking RequestContext by value
                return [handler = std::forward<F>(handler)](
                           RequestContext ctx) -> AsyncResponse { co_return handler(std::move(ctx)); };
            } else if constexpr (std::is_invocable_r_v<Response, F, const RequestContext&>) {
                // Sync handler taking RequestContext by const reference
                return [handler = std::forward<F>(handler)](
                           RequestContext ctx) -> AsyncResponse { co_return handler(ctx); };
            } else if constexpr (std::is_invocable_r_v<AsyncResponse, F, RequestContext>) {
                // Async handler taking RequestContext by value
                return [handler = std::forward<F>(handler)](
                           RequestContext ctx) -> AsyncResponse { co_return co_await handler(std::move(ctx)); };
            } else if constexpr (std::is_invocable_r_v<AsyncResponse, F, const RequestContext&>) {
                // Async handler taking RequestContext by const reference
                return [handler = std::forward<F>(handler)](
                           RequestContext ctx) -> AsyncResponse { co_return co_await handler(ctx); };
            } else {
                static_assert(false, "Handler must be callable with RequestContext or const RequestContext& and return "
                                     "Response or AsyncResponse");
                gears::unreachable<F>();
            }
        }
    };

} // namespace demiplane::http