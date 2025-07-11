#pragma once

#include <memory>

#include "aliases.hpp"
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

        // Async method binding
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

        // Lambda/function binding (fallback)
        void Get(std::string path, ContextHandler handler);
        void Post(std::string path, ContextHandler handler);
        void Put(std::string path, ContextHandler handler);
        void Delete(std::string path, ContextHandler handler);

    private:
        RouteRegistry registry_;

        // Simple binding helpers - no templates needed
        template <IsController Controller>
        ContextHandler bind_sync_method(Response (Controller::*method)(RequestContext)) {
            auto self = std::static_pointer_cast<Controller>(shared_from_this());
            return [self, method](RequestContext ctx) -> AsyncResponse {
                co_return (self.get()->*method)(std::move(ctx));
            };
        }

        template <IsController Controller>
        ContextHandler bind_async_method(AsyncResponse (Controller::*method)(RequestContext)) {
            auto self = std::static_pointer_cast<Controller>(shared_from_this());
            return [self, method](RequestContext ctx) -> AsyncResponse {
                // TODO: Caught ICE
                co_return co_await (self.get()->*method)(std::move(ctx));
            };
        }
    };

} // namespace demiplane::http
