#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <boost/asio/io_context.hpp>
#include <gtest/gtest.h>

#include <controller.hpp>
#include <http11_config.hpp>
#include <http11_driver.hpp>
#include <request_context.hpp>
#include <tls_config.hpp>
#include <tls_listener.hpp>

#include "http_test_fixture.hpp"
#include "test_tls_cert.hpp"
#include "tls_client.hpp"

using namespace demiplane::http;

namespace {

    class TlsApiController final : public HttpController {
    public:
        void configure_routes() override {
            Get("/hello", &TlsApiController::hello);
        }

    private:
        AsyncResponse hello(RequestContext ctx) {  // NOLINT(readability-convert-member-functions-to-static)
            co_return ctx.ok("hello tls");
        }
    };

    using http_it::TlsClient;

    class HttpTlsTest : public http_it::HttpIntegrationFixture {
    protected:
        void SetUp() override {
            const std::string cert = http_tls_test::write_temp("cert.pem", http_tls_test::kTestCertPem);
            const std::string key  = http_tls_test::write_temp("key.pem", http_tls_test::kTestKeyPem);
            const auto tls = TlsConfig::Builder{}.cert_file(cert).key_file(key).finalize();

            add_controller(std::make_shared<TlsApiController>());
            start(std::make_unique<TlsListener<Http11Driver>>(
                ioc_.get_executor(), "127.0.0.1", 0, tls, Http11Driver{Http11Config{}}));
        }
    };

}  // namespace

TEST_F(HttpTlsTest, HandshakeNegotiatesHttp11AndRoundTrips) {
    TlsClient client;
    const auto ec = client.connect_handshake(port_, {"h2", "http/1.1"});
    ASSERT_FALSE(ec) << ec.message();
    EXPECT_EQ(client.negotiated_alpn(), "http/1.1");

    auto res = client.get("/hello");
    EXPECT_EQ(res.result_int(), 200u);
    EXPECT_EQ(res.body(), "hello tls");
}

TEST_F(HttpTlsTest, ClientOfferingOnlyH2IsRejected) {
    TlsClient client;
    // The h1-only listener advertises only "http/1.1"; an h2-only offer has no
    // overlap → server returns ALERT_FATAL → handshake fails (spike S2 / D4).
    const auto ec = client.connect_handshake(port_, {"h2"});
    EXPECT_TRUE(ec) << "handshake should fail when no ALPN protocol overlaps";
}
