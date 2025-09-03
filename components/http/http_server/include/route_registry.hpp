#pragma once

#include <demiplane/nexus>
#include <memory>
#include <regex>
#include <string>
#include <vector>

#include <boost/unordered/unordered_flat_map.hpp>

#include "aliases.hpp"

namespace demiplane::http {

    struct RouteInfo {
        boost::beast::http::verb method = boost::beast::http::verb::unknown;
        std::string path;
        std::string pattern;
        std::vector<std::string> param_names;
        ContextHandler handler;
        bool is_parametric                         = false;
        std::shared_ptr<std::regex> compiled_regex = nullptr;
    };

    class RouteRegistry {
    public:
        NEXUS_REGISTER(0xF6A865A4, nexus::Resettable);  // CRC32/ISO-HDLC of demiplane::http::RouteRegistry

        void add_route(boost::beast::http::verb method, std::string path, ContextHandler handler);

        // Merge another registry into this one
        void merge(RouteRegistry&& other);
        void merge(const RouteRegistry& other);

        [[nodiscard]] std::pair<ContextHandler, std::unordered_map<std::string, std::string>>
        find_handler(boost::beast::http::verb method, const std::string& path) const;

        [[nodiscard]] size_t route_count() const;
        void clear();

        // Move operations for efficient transfer
        RouteRegistry()                           = default;
        RouteRegistry(RouteRegistry&&)            = default;
        RouteRegistry& operator=(RouteRegistry&&) = default;

        // Copy operations
        RouteRegistry(const RouteRegistry& other);
        RouteRegistry& operator=(const RouteRegistry& other);

    private:
        boost::unordered::unordered_flat_map<std::string, ContextHandler> exact_routes_;
        std::vector<RouteInfo> parametric_routes_;

        [[nodiscard]] static std::string make_route_key(boost::beast::http::verb method, const std::string& path);
        static RouteInfo
        create_parametric_route(boost::beast::http::verb method, std::string path, ContextHandler handler);
        [[nodiscard]] static bool is_parametric_path(const std::string& path);
    };

}  // namespace demiplane::http
