#include <string>

#include <boost/asio/ssl/context.hpp>
#include <gtest/gtest.h>

#include <build_ssl_context.hpp>
#include <tls_config.hpp>

#include "test_tls_cert.hpp"

using namespace demiplane::http;

namespace {
    std::string alpn_wire_http11() {
        std::string wire;
        wire.push_back('\x08');   // length of "http/1.1"
        wire += "http/1.1";
        return wire;
    }
}  // namespace

TEST(BuildSslContextTest, BuildsFromValidCert) {
    const auto cfg = TlsConfig::Builder{}
                         .cert_file(http_tls_test::write_temp("cert.pem", http_tls_test::kTestCertPem))
                         .key_file(http_tls_test::write_temp("key.pem", http_tls_test::kTestKeyPem))
                         .finalize();

    const std::string advertised = alpn_wire_http11();
    auto ctx = build_ssl_context(cfg, advertised);  // must not throw
    EXPECT_NE(ctx.native_handle(), nullptr);
}

TEST(BuildSslContextTest, ThrowsOnMissingCert) {
    const auto cfg = TlsConfig::Builder{}
                         .cert_file("/nonexistent/path/cert.pem")
                         .key_file("/nonexistent/path/key.pem")
                         .finalize();
    const std::string advertised = alpn_wire_http11();
    // TODO: tighten to EXPECT_THROW(..., boost::system::system_error) to match build_ssl_context's documented throw type.
    EXPECT_ANY_THROW({ auto ctx = build_ssl_context(cfg, advertised); });
}
