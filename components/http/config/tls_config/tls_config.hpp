#pragma once

#include <cstdint>
#include <string>

namespace demiplane::http {

    /**
     * @brief TLS settings consumed by build_ssl_context (spec §7.4 / §10.1).
     *
     * PR 4 ships this as a PLAIN STRUCT (D1) — the serialization::ConfigInterface
     * version (with fields()/Builder/validate()) lands in PR 6, which rewrites
     * THIS file in place; build_ssl_context then switches `.cert_file` to
     * `.cert_file()`. Kept at the spec's final path so the include is stable.
     */
    struct TlsConfig {
        enum class MinVersion : std::uint8_t { tls12, tls13 };

        std::string cert_file;       // PEM cert chain (required)
        std::string key_file;        // PEM private key (required)
        std::string key_passphrase;  // optional; empty → no passphrase callback
        std::string dh_params_file;  // optional DH params
        std::string ca_file;         // optional; required if require_client_cert

        MinVersion min_version   = MinVersion::tls12;
        bool session_cache       = true;
        bool require_client_cert = false;
    };

}  // namespace demiplane::http
