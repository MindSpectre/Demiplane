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
    TlsConfig tls;  // empty cert paths — fine, bind() (which builds the ctx) is not called here
    TlsListener<Http11Driver> listener{ioc.get_executor(), "127.0.0.1", 0, tls,
                                       Http11Driver{Http11Config{}}};
    EXPECT_EQ(listener.bind_address(), "127.0.0.1");
}
