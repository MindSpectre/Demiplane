#pragma once
#include <vector>

#include "tls_config.hpp"
#include <boost/unordered/unordered_flat_map.hpp>
namespace demiplane::http {

    struct listener {
        std::string address = "0.0.0.0"; // IPv4 / IPv6 literal or "*" for any
        std::uint16_t port  = 8080;
        bool tls            = false; // enable TLS handshake on this socket
    };

    struct timeouts {
        std::chrono::milliseconds handshake = std::chrono::seconds{10}; // TLS handshake or first byte
        std::chrono::milliseconds header    = std::chrono::seconds{10}; // read headers
        std::chrono::milliseconds body      = std::chrono::seconds{30}; // read body / send resp
        std::chrono::milliseconds idle      = std::chrono::seconds{60}; // keep‑alive
    };


    struct route_flags {
        bool enabled = true; // false → framework returns 404/410
    };

    using route_table = boost::unordered::unordered_flat_map<std::string /*path*/, route_flags>;
    // -----------------------------------------------------------------------------
    //  Top‑level server configuration
    // -----------------------------------------------------------------------------
    struct server {
        std::vector<listener> listeners; // can expose http & https separately
        std::size_t io_threads = 1;

        timeouts to; // global timeouts
        tls_settings tls; // shared cert store (if any)

        // IP‑based rules; evaluated before route dispatch
        std::vector<ip_rule> ip_limits;

        // Per‑route enable/disable switches (populated from YAML)
        route_table routes;
    };

    // -----------------------------------------------------------------------------
    //  YAML parsing stubs (no impl yet)
    // -----------------------------------------------------------------------------
    [[nodiscard]] server load_from_yaml(std::string_view yaml_path);
    [[nodiscard]] std::string dump_to_yaml(const server& cfg);

    // -----------------------------------------------------------------------------
    //  Hot‑reload helper – returns true if the YAML differs and cfg was updated
    // -----------------------------------------------------------------------------
    [[nodiscard]] bool reload_if_changed(server& cfg, std::string_view yaml_path);

} // namespace demiplane::http
