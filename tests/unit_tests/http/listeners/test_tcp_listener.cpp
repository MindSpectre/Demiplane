#include <concepts>

#include <boost/asio/io_context.hpp>
#include <gtest/gtest.h>

#include <http11_config.hpp>
#include <http11_driver.hpp>
#include <listener_base.hpp>
#include <tcp_listener.hpp>

using namespace demiplane::http;

// TcpListener<Driver> is the polymorphic seam the Server holds (spec §7.1).
static_assert(std::derived_from<TcpListener<Http11Driver>, ListenerBase>);

// TODO(PR5): add an integration test that bind() throws boost::system::system_error on EADDRINUSE (bind two listeners to the same fixed port) — §14.2 lifecycle.

TEST(TcpListenerTest, ConstructsAndReportsBindAddress) {
    boost::asio::io_context ioc;
    TcpListener<Http11Driver> listener{ioc.get_executor(), "127.0.0.1", 0,
                                       Http11Driver{Http11Config{}}};
    // No bind() here — opening a real socket / accepting is integration (Task 6).
    EXPECT_EQ(listener.bind_address(), "127.0.0.1");
}
