#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>

namespace demiplane::http {

    /**
     * @brief Server-level tuning consumed by the Server (spec §10.1 subset).
     *
     * PR 5 ships this as a PLAIN STRUCT (PR 4 D1 precedent: TlsConfig) — the
     * serialization::ConfigInterface version (fields()/Builder/validate(),
     * plus listeners/threads/timeouts/body_limit) lands in PR 6, which
     * rewrites THIS file in place. Kept at the spec's final path so the
     * include is stable.
     */
    struct ServerConfig {
        /// Mirrors routing's PathNormalization (route_registry.hpp). The
        /// Server maps this config enum onto the routing enum so the config
        /// layer carries no routing dependency (route_registry.hpp:23 staging).
        enum class PathNormalization : std::uint8_t {
            none,                     ///< exact byte match
            collapse_trailing_slash,  ///< "/users/" == "/users"   (default)
            collapse_multi_slash,     ///< + "/users//42" == "/users/42"
        };

        /// Per-connection request arena block (spec §6.1); forwarded to
        /// TcpListener. TlsListener stays at its fixed 8 KB default until
        /// PR 6 (PR 4 note in tls_listener.hpp).
        std::size_t request_arena_size = 8192;

        /// Budget for graceful_shutdown()'s drain phase (spec §9.5 phase 2):
        /// in-flight requests get this long to finish before force-cancel.
        std::chrono::milliseconds drain_timeout{std::chrono::seconds{30}};

        PathNormalization path_normalization = PathNormalization::collapse_trailing_slash;
    };

}  // namespace demiplane::http
