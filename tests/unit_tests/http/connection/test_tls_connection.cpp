#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/strand.hpp>
#include <gtest/gtest.h>

#include <connection_concepts.hpp>
#include <executor.hpp>
#include <tls_connection.hpp>

using namespace demiplane::http;

static_assert(IsStreamConnection<TlsConnection>);

TEST(TlsConnectionTest, MetadataDefaults) {
    boost::asio::io_context ioc;
    boost::asio::ssl::context ctx{boost::asio::ssl::context::tls_server};
    Socket sock{ioc.get_executor()};
    TlsConnection conn{std::move(sock), ctx};
    EXPECT_TRUE(conn.is_secure());
    // Before the handshake, the negotiated protocol defaults to http1.
    EXPECT_EQ(conn.negotiated_protocol(), Protocol::http1);
    EXPECT_NE(conn.arena_alloc().resource(), nullptr);
    conn.reset_request_arena();  // does not throw on a fresh arena
    conn.cancel();               // no slot connected yet → safe no-op
}
