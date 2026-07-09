#include <memory>
#include <string>

#include <gtest/gtest.h>

#include <attach_default_listeners.hpp>
#include <controller.hpp>
#include <load_server_config.hpp>
#include <request_context.hpp>
#include <server.hpp>

#include "server_test_fixture.hpp"
#include "test_tls_cert.hpp"
#include "tls_client.hpp"

using namespace demiplane::http;
namespace bhttp = boost::beast::http;

namespace {

    class HelloController final : public HttpController {
    public:
        void configure_routes() override {
            Get("/hello", &HelloController::hello);
        }

    private:
        AsyncResponse hello(RequestContext ctx) {  // NOLINT(readability-convert-member-functions-to-static)
            co_return ctx.ok("hello config");
        }
    };

    class ConfigWiringTest : public http_it::ServerIntegrationFixture {};

}  // namespace

// §12.2 acceptance: a JSON file becomes a serving listener in one
// load + one attach. body_limit=64 must reach the driver (413 pre-routing).
TEST_F(ConfigWiringTest, TcpListenerFromJsonServesAndEnforcesBodyLimit) {
    const auto path = http_tls_test::write_temp("wiring_tcp.json", R"({
        "threads": 1,
        "body_limit": 64,
        "request_arena_size": 8192,
        "drain_timeout_ms": 2000,
        "timeouts": { "header_ms": 5000, "body_ms": 5000, "idle_ms": 5000 },
        "listeners": [
            { "bind": "127.0.0.1", "port": 0, "transport": "tcp", "protocols": ["http1"] }
        ]
    })");
    auto loaded = load_server_config(path);
    ASSERT_TRUE(loaded.is_success());

    start_server(
        [](Server& s) {
            s.add_controller(std::make_shared<HelloController>());
            attach_default_listeners(s);
        },
        std::move(loaded).value());

    http_it::TcpClient ok_client{port()};
    const auto ok_res = ok_client.send(bhttp::verb::get, "/hello");
    EXPECT_EQ(ok_res.result_int(), 200u);
    EXPECT_EQ(ok_res.body(), "hello config");

    // Oversize Content-Length → the driver's eager 413, before routing runs.
    http_it::TcpClient big_client{port()};
    const auto big = big_client.send(bhttp::verb::post, "/hello", std::string(100, 'x'));
    EXPECT_EQ(big.result_int(), 413u);
}

// TLS material from JSON: cert/key paths land in build_ssl_context and ALPN
// negotiates http/1.1 on the wire.
TEST_F(ConfigWiringTest, TlsListenerFromJsonNegotiatesH1) {
    const std::string cert = http_tls_test::write_temp("wiring_cert.pem", http_tls_test::kTestCertPem);
    const std::string key  = http_tls_test::write_temp("wiring_key.pem", http_tls_test::kTestKeyPem);
    const std::string json = R"({
        "listeners": [
            { "bind": "127.0.0.1", "port": 0, "transport": "tls", "protocols": ["http1"],
              "tls": { "cert_file": ")" +
                             cert + R"(", "key_file": ")" + key + R"(" } }
        ]
    })";
    const auto path = http_tls_test::write_temp("wiring_tls.json", json);
    auto loaded     = load_server_config(path);
    ASSERT_TRUE(loaded.is_success());

    start_server(
        [](Server& s) {
            s.add_controller(std::make_shared<HelloController>());
            attach_default_listeners(s);
        },
        std::move(loaded).value());

    http_it::TlsClient client;
    const auto ec = client.connect_handshake(port(), {"http/1.1"});
    ASSERT_FALSE(ec) << ec.message();
    EXPECT_EQ(client.negotiated_alpn(), "http/1.1");
    const auto res = client.get("/hello");
    EXPECT_EQ(res.result_int(), 200u);
    EXPECT_EQ(res.body(), "hello config");
}
