#pragma once

#include <chrono>
#include <cstddef>

namespace demiplane::http {

    /// Per-driver HTTP/1.1 limits + phase timeouts (spec §6.3).
    /// attach_default_listeners derives one from ServerConfig (body_limit +
    /// phase timeouts); direct construction remains for per-driver tuning.
    struct Http11Config {
        std::size_t max_header_bytes = 16 * 1024;
        std::size_t max_body_bytes   = 16 * 1024 * 1024;

        std::chrono::milliseconds header_timeout = std::chrono::seconds{10};
        std::chrono::milliseconds body_timeout   = std::chrono::seconds{30};
        // Bounds the keep-alive wait for the NEXT request (empty input buffer
        // at message start). header_timeout takes over once a message is
        // mid-arrival; body_timeout once its header completes.
        std::chrono::milliseconds idle_timeout   = std::chrono::seconds{60};
    };

}  // namespace demiplane::http
