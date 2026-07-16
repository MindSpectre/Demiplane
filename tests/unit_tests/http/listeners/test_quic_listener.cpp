#include <concepts>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <gtest/gtest.h>

#include <http3_driver.hpp>
#include <listener_base.hpp>
#include <quic_listener.hpp>
#include <route_registry.hpp>
#include <router.hpp>
#include <tls_config.hpp>

using namespace demiplane::http;

static_assert(std::derived_from<QuicListener<Http3Driver>, ListenerBase>);

TEST(QuicListenerTest, ScaffoldBindsAndRunsImmediately) {
    boost::asio::io_context ioc;
    QuicListener<Http3Driver> listener{
        std::vector{ioc.get_executor()},
        "127.0.0.1", 8443, TlsConfig{"", ""},
        Http3Driver{}};
    listener.bind();  // no-op success
    EXPECT_EQ(listener.bind_address(), "127.0.0.1");
    EXPECT_EQ(listener.bound_port(), 8443u);

    RouteRegistry registry;
    ASSERT_TRUE(registry.freeze().empty());
    Router router{registry};
    boost::asio::co_spawn(ioc, listener.run(router), boost::asio::detached);
    ioc.run();  // run() co_returns immediately → io_context drains and returns
    SUCCEED();
}
