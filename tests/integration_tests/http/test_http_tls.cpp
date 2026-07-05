#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <boost/asio/connect.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/http.hpp>
#include <openssl/ssl.h>
#include <gtest/gtest.h>

#include <controller.hpp>
#include <http11_config.hpp>
#include <http11_driver.hpp>
#include <request_context.hpp>
#include <tls_config.hpp>
#include <tls_listener.hpp>

#include "http_test_fixture.hpp"
#include "test_tls_cert.hpp"

using namespace demiplane::http;
namespace asio  = boost::asio;
namespace bhttp = boost::beast::http;

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

    // Synchronous Beast TLS client. connect_handshake() sets the ALPN offer and
    // returns the handshake error_code (empty = success).
    class TlsClient {
    public:
        TlsClient() : stream_{ioc_, ctx_} {
            ctx_.set_verify_mode(asio::ssl::verify_none);  // self-signed test cert
        }

        boost::beast::error_code connect_handshake(std::uint16_t port,
                                                   const std::vector<std::string_view>& alpns) {
            std::string wire;
            for (const auto a : alpns) {
                wire.push_back(static_cast<char>(a.size()));
                wire.append(a);
            }
            // TODO: check SSL_set_alpn_protos return (0 == success); the stream_.handshake(...) tidy warning is benign (ec is used) — silence with std::ignore/NOLINT if desired.
            ::SSL_set_alpn_protos(stream_.native_handle(),
                                  reinterpret_cast<const unsigned char*>(wire.data()),
                                  static_cast<unsigned int>(wire.size()));
            stream_.next_layer().connect({asio::ip::make_address("127.0.0.1"), port});
            boost::beast::error_code ec;
            stream_.handshake(asio::ssl::stream_base::client, ec);
            return ec;
        }

        [[nodiscard]] std::string negotiated_alpn() {
            const unsigned char* proto = nullptr;
            unsigned int len           = 0;
            ::SSL_get0_alpn_selected(stream_.native_handle(), &proto, &len);
            return std::string{reinterpret_cast<const char*>(proto), len};
        }

        http_it::ParsedResponse get(const std::string& target) {
            bhttp::request<bhttp::string_body> req{bhttp::verb::get, target, 11};
            req.set(bhttp::field::host, "127.0.0.1");
            req.keep_alive(false);
            req.prepare_payload();
            bhttp::write(stream_, req);
            http_it::ParsedResponse res;
            boost::beast::flat_buffer buf;
            bhttp::read(stream_, buf, res);
            return res;
        }

    private:
        asio::io_context ioc_;
        asio::ssl::context ctx_{asio::ssl::context::tls_client};
        asio::ssl::stream<asio::ip::tcp::socket> stream_;
    };

    class HttpTlsTest : public http_it::HttpIntegrationFixture {
    protected:
        void SetUp() override {
            const std::string cert = http_tls_test::write_temp("cert.pem", http_tls_test::kTestCertPem);
            const std::string key  = http_tls_test::write_temp("key.pem", http_tls_test::kTestKeyPem);
            TlsConfig tls;
            tls.cert_file = cert;
            tls.key_file  = key;

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
