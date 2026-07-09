#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/http.hpp>
#include <openssl/ssl.h>

#include "http_test_fixture.hpp"

namespace http_it {

    // Synchronous Beast TLS client (extracted from test_http_tls.cpp so
    // config-wiring tests can reuse it). connect_handshake() sets the ALPN
    // offer and returns the handshake error_code (empty = success).
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

        ParsedResponse get(const std::string& target) {
            bhttp::request<bhttp::string_body> req{bhttp::verb::get, target, 11};
            req.set(bhttp::field::host, "127.0.0.1");
            req.keep_alive(false);
            req.prepare_payload();
            bhttp::write(stream_, req);
            ParsedResponse res;
            boost::beast::flat_buffer buf;
            bhttp::read(stream_, buf, res);
            return res;
        }

    private:
        asio::io_context ioc_;
        asio::ssl::context ctx_{asio::ssl::context::tls_client};
        asio::ssl::stream<asio::ip::tcp::socket> stream_;
    };

}  // namespace http_it
