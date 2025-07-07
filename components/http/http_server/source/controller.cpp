#include "controller.hpp"

namespace demiplane::http {

    void HttpController::transfer_routes_to(RouteRegistry& target_registry) {
        target_registry.merge(std::move(registry_));
    }

    size_t HttpController::route_count() const {
        return registry_.route_count();
    }

    // Lambda/function binding
    void HttpController::Get(std::string path, ContextHandler handler) {
        registry_.add_route(boost::beast::http::verb::get, std::move(path), std::move(handler));
    }

    void HttpController::Post(std::string path, ContextHandler handler) {
        registry_.add_route(boost::beast::http::verb::post, std::move(path), std::move(handler));
    }

    void HttpController::Put(std::string path, ContextHandler handler) {
        registry_.add_route(boost::beast::http::verb::put, std::move(path), std::move(handler));
    }

    void HttpController::Delete(std::string path, ContextHandler handler) {
        registry_.add_route(boost::beast::http::verb::delete_, std::move(path), std::move(handler));
    }


} // namespace demiplane::http
