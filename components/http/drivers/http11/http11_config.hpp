#pragma once

#include <chrono>
#include <cstddef>

namespace demiplane::http {

    /// Per-driver HTTP/1.1 limits + phase timeouts (spec §6.3). ServerConfig
    /// (PR 6) constructs these from loaded config; for now callers build them
    /// directly.
    struct Http11Config {
        std::size_t max_header_bytes = 16 * 1024;
        std::size_t max_body_bytes   = 16 * 1024 * 1024;

        std::chrono::milliseconds header_timeout = std::chrono::seconds{10};
        std::chrono::milliseconds body_timeout   = std::chrono::seconds{30};
        // Reserved: in v1 the keep-alive idle wait reuses header_timeout (the
        // driver does not yet honor a separate idle_timeout).
        std::chrono::milliseconds idle_timeout   = std::chrono::seconds{60};
    };

}  // namespace demiplane::http
