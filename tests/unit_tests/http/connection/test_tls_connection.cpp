#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>
#include <gtest/gtest.h>

#include <connection_concepts.hpp>
#include <tls_connection.hpp>

using namespace demiplane::http;

static_assert(StreamConnection<TlsConnection>);

TEST(TlsConnectionTest, MetadataDefaults) {
    boost::asio::io_context ioc;
    boost::asio::ssl::context ctx{boost::asio::ssl::context::tls_server};
    boost::asio::ip::tcp::socket sock{ioc};
    TlsConnection conn{std::move(sock), ctx};
    EXPECT_TRUE(conn.is_secure());
    // Before the handshake, the negotiated protocol defaults to http1.
    EXPECT_EQ(conn.negotiated_protocol(), Protocol::http1);
    EXPECT_NE(conn.arena_alloc().resource(), nullptr);
    conn.reset_request_arena();  // does not throw on a fresh arena
    conn.cancel();               // no slot connected yet → safe no-op
}
