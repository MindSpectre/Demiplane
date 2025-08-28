#include "route_registry.hpp"

#include <regex>
#include <demiplane/scroll>

namespace demiplane::http {

    RouteRegistry::RouteRegistry(const RouteRegistry& other)
        : exact_routes_(other.exact_routes_), parametric_routes_(other.parametric_routes_) {
        // Re-compile regex patterns for parametric routes
        for (auto& route : parametric_routes_) {
            if (route.is_parametric) {
                route.compiled_regex = std::make_shared<std::regex>(route.pattern);
            }
        }
    }

    RouteRegistry& RouteRegistry::operator=(const RouteRegistry& other) {
        if (this != &other) {
            exact_routes_      = other.exact_routes_;
            parametric_routes_ = other.parametric_routes_;

            // Re-compile regex patterns
            for (auto& route : parametric_routes_) {
                if (route.is_parametric) {
                    route.compiled_regex = std::make_shared<std::regex>(route.pattern);
                }
            }
        }
        return *this;
    }

    void RouteRegistry::merge(RouteRegistry&& other) {
        // Move exact routes
        for (auto& [key, handler] : other.exact_routes_) {
            exact_routes_[key] = std::move(handler);
        }

        // Move parametric routes
        for (auto& route : other.parametric_routes_) {
            parametric_routes_.push_back(std::move(route));
        }

        other.clear();
    }

    void RouteRegistry::merge(const RouteRegistry& other) {
        // Copy exact routes
        for (const auto& [key, handler] : other.exact_routes_) {
            exact_routes_[key] = handler;
        }

        // Copy parametric routes
        for (const auto& route : other.parametric_routes_) {
            parametric_routes_.push_back(route);

            // Re-compile regex for the copied route
            if (route.is_parametric) {
                parametric_routes_.back().compiled_regex = std::make_shared<std::regex>(route.pattern);
            }
        }
    }

    void RouteRegistry::add_route(const boost::beast::http::verb method, std::string path, ContextHandler handler) {
        if (is_parametric_path(path)) {
            COMPONENT_LOG_INF() << "Adding parametric route" << SCROLL_PARAMS(method, path);
            parametric_routes_.push_back(create_parametric_route(method, std::move(path), std::move(handler)));
        } else {
            COMPONENT_LOG_INF() << "Adding route" << SCROLL_PARAMS(method, path);
            const auto key     = make_route_key(method, path);
            exact_routes_[key] = std::move(handler);
        }
    }

    std::pair<ContextHandler, std::unordered_map<std::string, std::string>> RouteRegistry::find_handler(
        const boost::beast::http::verb method, const std::string& path) const {
        // Try exact match first
        const auto key = make_route_key(method, path);
        if (const auto it = exact_routes_.find(key); it != exact_routes_.end()) {
            COMPONENT_LOG_DBG() << "Found exact route" << SCROLL_PARAMS(method, path);
            return std::make_pair(it->second, std::unordered_map<std::string, std::string>{});
        }

        // Try parametric routes
        for (const auto& route : parametric_routes_) {
            if (route.method != method) {
                continue;
            }

            if (std::smatch match; std::regex_match(path, match, *route.compiled_regex)) {
                std::unordered_map<std::string, std::string> params;
                for (size_t i = 0; i < route.param_names.size() && i + 1 < match.size(); ++i) {
                    params[route.param_names[i]] = match[i + 1].str();
                }
                COMPONENT_LOG_DBG() << "Found parametric route" << SCROLL_PARAMS(method, path);
                return std::make_pair(route.handler, params);
            }
        }
        COMPONENT_LOG_WRN() << "No route found for" << SCROLL_PARAMS(method, path);
        throw std::out_of_range("No route found for " + std::to_string(static_cast<int>(method)) + " " + path);
    }

    size_t RouteRegistry::route_count() const {
        return exact_routes_.size() + parametric_routes_.size();
    }

    void RouteRegistry::clear() {
        COMPONENT_LOG_DBG() << "Clearing routes";
        exact_routes_.clear();
        parametric_routes_.clear();
    }

    std::string RouteRegistry::make_route_key(boost::beast::http::verb method, const std::string& path) {
        return std::to_string(static_cast<int>(method)) + ":" + path;
    }

    bool RouteRegistry::is_parametric_path(const std::string& path) {
        return path.find('{') != std::string::npos;
    }

    RouteInfo RouteRegistry::create_parametric_route(
        const boost::beast::http::verb method, std::string path, ContextHandler handler) {
        COMPONENT_LOG_DBG() << "Creating parametric route" << SCROLL_PARAMS(method, path);
        RouteInfo info;
        info.method        = method;
        info.path          = path;
        info.handler       = std::move(handler);
        info.is_parametric = true;

        const std::regex param_regex(R"(\{([^}]+)\})");
        std::string pattern = std::move(path);
        std::smatch match;

        auto pattern_copy = pattern;
        while (std::regex_search(pattern_copy, match, param_regex)) {
            info.param_names.push_back(match[1].str());
            pattern_copy = match.suffix();
        }

        pattern             = std::regex_replace(pattern, param_regex, "([^/]+)");
        info.pattern        = "^" + pattern + "$";
        info.compiled_regex = std::make_shared<std::regex>(info.pattern);
        COMPONENT_LOG_DBG() << "Parametric route created" << SCROLL_PARAMS(info.pattern);
        return info;
    }

} // namespace demiplane::http
