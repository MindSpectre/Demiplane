#pragma once

namespace demiplane::http {
    struct tls_settings {
        std::string cert_file;  // PEM
        std::string key_file;   // PEM
        std::string dh_file;    // optional DH params

        enum class min_version { tls12, tls13 };
        min_version minimum = min_version::tls12;
        bool session_cache  = true;   // enable session IDs / tickets
        bool enable_ocsp    = false;  // stapled OCSP response (future)
    };
}  // namespace demiplane::http
