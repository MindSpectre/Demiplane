#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <gtest/gtest.h>

#include <connection_concepts.hpp>
#include <tcp_connection.hpp>

using namespace demiplane::http;

static_assert(IsStreamConnection<TcpConnection>);

TEST(TcpConnectionTest, MetadataDefaults) {
    boost::asio::io_context ioc;
    boost::asio::ip::tcp::socket sock{ioc};
    TcpConnection conn{std::move(sock)};
    EXPECT_EQ(conn.negotiated_protocol(), Protocol::http1);
    EXPECT_FALSE(conn.is_secure());
    EXPECT_NE(conn.arena_alloc().resource(), nullptr);
    conn.reset_request_arena();  // does not throw on a fresh arena
}

TEST(TcpConnectionTest, CancelOnFreshConnectionIsHarmless) {
    boost::asio::io_context ioc;
    boost::asio::ip::tcp::socket sock{ioc};
    TcpConnection conn{std::move(sock)};
    conn.cancel();  // no slot connected yet → safe no-op, must not throw/crash
    SUCCEED();
}
