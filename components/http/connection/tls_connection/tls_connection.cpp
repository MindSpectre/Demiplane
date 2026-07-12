#include "tls_connection.hpp"

#include <string_view>
#include <tuple>

#include <boost/asio/redirect_error.hpp>
#include <boost/asio/ssl/stream_base.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <openssl/ssl.h>

namespace demiplane::http {

    namespace {
        Protocol protocol_from_alpn(std::string_view alpn) noexcept {
            if (alpn == "h2") {
                return Protocol::http2;
            }
            if (alpn == "h3") {
                return Protocol::http3;
            }
            return Protocol::http1;  // "http/1.1" or (defensively) none selected
        }
    }  // namespace

    boost::asio::awaitable<boost::beast::error_code, Strand>
    TlsConnection::handshake(std::chrono::milliseconds timeout) {
        boost::beast::get_lowest_layer(stream_).expires_after(timeout);

        boost::beast::error_code ec;
        co_await stream_.async_handshake(boost::asio::ssl::stream_base::server,
                                         boost::asio::redirect_error(use_strand_awaitable, ec));
        if (ec) {
            co_return ec;
        }

        const unsigned char* proto = nullptr;
        unsigned int len           = 0;
        ::SSL_get0_alpn_selected(stream_.native_handle(), &proto, &len);
        negotiated_protocol_ = protocol_from_alpn(std::string_view{reinterpret_cast<const char*>(proto), len});
        co_return ec;  // empty (success)
    }

    boost::asio::awaitable<void, Strand> TlsConnection::async_close() {
        boost::beast::get_lowest_layer(stream_).expires_after(std::chrono::seconds{5});
        boost::beast::error_code ec;
        // Best-effort TLS close-notify; peer may already be gone.
        co_await stream_.async_shutdown(boost::asio::redirect_error(use_strand_awaitable, ec));
        boost::beast::get_lowest_layer(stream_).socket().shutdown(boost::asio::ip::tcp::socket::shutdown_send,
                                                                  ec);  // void under BOOST_ASIO_NO_DEPRECATED
        co_return;
    }

}  // namespace demiplane::http
