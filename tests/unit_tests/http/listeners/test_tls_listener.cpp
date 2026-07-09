#include <concepts>

#include <boost/asio/io_context.hpp>
#include <gtest/gtest.h>

#include <http11_config.hpp>
#include <http11_driver.hpp>
#include <listener_base.hpp>
#include <tls_config.hpp>
#include <tls_listener.hpp>

using namespace demiplane::http;

static_assert(std::derived_from<TlsListener<Http11Driver>, ListenerBase>);

TEST(TlsListenerTest, ConstructsWithoutBinding) {
    boost::asio::io_context ioc;
    TlsConfig tls{"", ""};  // empty cert paths via the full-ctor escape hatch — fine,
                            // bind() (which builds the ctx) is not called here
    TlsListener<Http11Driver> listener{ioc.get_executor(), "127.0.0.1", 0, tls,
                                       Http11Driver{Http11Config{}}};
    EXPECT_EQ(listener.bind_address(), "127.0.0.1");
}

TEST(TlsListenerTest, ConstructsWithExplicitArenaSize) {
    boost::asio::io_context ioc;
    TlsConfig tls{"", ""};  // full-ctor escape hatch; bind() (ctx build) is not called
    TlsListener<Http11Driver> listener{
        ioc.get_executor(), "127.0.0.1", 0, tls, 4096, Http11Driver{Http11Config{}}};
    EXPECT_EQ(listener.bind_address(), "127.0.0.1");
}
