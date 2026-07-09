#include "build_ssl_context.hpp"

#include <cstddef>

#include <boost/asio/ssl/error.hpp>
#include <boost/system/system_error.hpp>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/tls1.h>

namespace demiplane::http {

    namespace {
        // OpenSSL ALPN select callback (spike S2). `arg` is the advertised
        // length-prefixed protocol list (the listener's long-lived buffer, D4).
        int alpn_select_cb(SSL* /*ssl*/,
                           const unsigned char** out,
                           unsigned char* outlen,
                           const unsigned char* in,
                           unsigned int inlen,
                           void* arg) {
            const auto* advertised = static_cast<const std::string*>(arg);
            if (::SSL_select_next_proto(const_cast<unsigned char**>(out),
                                        outlen,
                                        reinterpret_cast<const unsigned char*>(advertised->data()),
                                        static_cast<unsigned int>(advertised->size()),
                                        in,
                                        inlen) == OPENSSL_NPN_NEGOTIATED) {
                return SSL_TLSEXT_ERR_OK;
            }
            // OpenSSL 3.6.3 has no SSL_TLSEXT_ERR_ALPN_FAILED — ALERT_FATAL aborts
            // the handshake with no_application_protocol (spike S2 / D4).
            return SSL_TLSEXT_ERR_ALERT_FATAL;
        }
    }  // namespace

    boost::asio::ssl::context build_ssl_context(const TlsConfig& cfg, const std::string& advertised_alpn_wire) {
        namespace ssl = boost::asio::ssl;

        ssl::context ctx{ssl::context::tls_server};

        auto options = ssl::context::default_workarounds | ssl::context::no_sslv2 | ssl::context::no_sslv3 |
                       ssl::context::no_tlsv1 | ssl::context::no_tlsv1_1 | ssl::context::no_compression |
                       ssl::context::single_dh_use;
        if (cfg.min_version() == TlsConfig::MinVersion::tls13) {
            options |= ssl::context::no_tlsv1_2;
        }
        ctx.set_options(options);

        // Modern TLS 1.2 cipher floor (TLS 1.3 suites use OpenSSL's safe defaults).
        // A silent 0 would fall back to OpenSSL defaults, weakening the floor
        // this function guarantees — so a failure throws.
        if (::SSL_CTX_set_cipher_list(ctx.native_handle(),
                                      "ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-AES128-GCM-SHA256:"
                                      "ECDHE-ECDSA-AES256-GCM-SHA384:ECDHE-RSA-AES256-GCM-SHA384:"
                                      "ECDHE-ECDSA-CHACHA20-POLY1305:ECDHE-RSA-CHACHA20-POLY1305") != 1) {
            throw boost::system::system_error{
                boost::system::error_code{static_cast<int>(::ERR_get_error()),
                                          boost::asio::error::get_ssl_category()},
                "SSL_CTX_set_cipher_list"};
        }

        if (!cfg.key_passphrase().empty()) {
            ctx.set_password_callback(
                [pass = cfg.key_passphrase()](std::size_t, ssl::context::password_purpose) { return pass; });
        }

        ctx.use_certificate_chain_file(cfg.cert_file());              // throws on failure
        ctx.use_private_key_file(cfg.key_file(), ssl::context::pem);  // throws on failure

        if (!cfg.dh_params_file().empty()) {
            ctx.use_tmp_dh_file(cfg.dh_params_file());
        }

        if (cfg.require_client_cert()) {
            ctx.set_verify_mode(ssl::verify_peer | ssl::verify_fail_if_no_peer_cert);
            if (!cfg.ca_file().empty()) {
                ctx.load_verify_file(cfg.ca_file());
            }
        }

        ::SSL_CTX_set_session_cache_mode(ctx.native_handle(),
                                         cfg.session_cache() ? SSL_SESS_CACHE_SERVER : SSL_SESS_CACHE_OFF);

        // ALPN: arg points at the CALLER'S buffer — must outlive ctx (D4).
        ::SSL_CTX_set_alpn_select_cb(
            ctx.native_handle(), &alpn_select_cb, const_cast<std::string*>(&advertised_alpn_wire));
        return ctx;
    }

}  // namespace demiplane::http
