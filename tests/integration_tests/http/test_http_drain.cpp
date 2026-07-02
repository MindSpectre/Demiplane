#include <chrono>
#include <memory>
#include <thread>

#include <boost/asio/steady_timer.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast/http/verb.hpp>
#include <gtest/gtest.h>

#include <controller.hpp>
#include <request_context.hpp>

#include "http_test_fixture.hpp"

using namespace demiplane::http;
namespace bhttp = boost::beast::http;
using namespace std::chrono_literals;

namespace {

    class DrainController final : public HttpController {
    public:
        void configure_routes() override {
            Get("/hello", &DrainController::hello);
            Get("/slow", &DrainController::slow);
        }

    private:
        AsyncResponse hello(RequestContext ctx) {
            co_return ctx.ok("hello world");
        }
        AsyncResponse slow(RequestContext ctx) {
            auto ex = co_await boost::asio::this_coro::executor;
            boost::asio::steady_timer t{ex};
            t.expires_after(150ms);
            co_await t.async_wait(boost::asio::use_awaitable);
            co_return ctx.ok("slow done");
        }
    };

    class HttpDrainTest : public http_it::HttpIntegrationFixture {
    protected:
        void SetUp() override {
            add_controller(std::make_shared<DrainController>());
            start(make_tcp_listener());
        }
    };

}  // namespace

// An in-flight request finishes while drain waits (drain timeout is generous).
TEST_F(HttpDrainTest, InFlightRequestCompletesDuringDrain) {
    int status = 0;
    std::string body;
    std::thread caller{[&] {
        http_it::TcpClient client{port_};
        auto res = client.send(bhttp::verb::get, "/slow");
        status = static_cast<int>(res.result_int());
        body   = res.body();
    }};

    std::this_thread::sleep_for(50ms);   // let the request reach the slow handler
    graceful_shutdown(2s);               // drain must wait for the in-flight /slow
    caller.join();

    EXPECT_EQ(status, 200);
    EXPECT_EQ(body, "slow done");
}

// An idle keep-alive connection (driver blocked reading the next request) is
// force-cancelled at the drain deadline; the server half-closes our socket.
TEST_F(HttpDrainTest, IdleKeepAliveConnectionForceCancelledAtDeadline) {
    http_it::TcpClient client{port_};
    auto res = client.send(bhttp::verb::get, "/hello", {}, "text/plain", /*keep_alive=*/true);
    ASSERT_EQ(res.result_int(), 200u);
    ASSERT_TRUE(res.keep_alive());
    // Connection now idle: the driver is awaiting the next request header.

    graceful_shutdown(100ms);            // short drain → force-cancel the idle conn

    const auto ec = client.read_after_close();
    EXPECT_TRUE(ec) << "server should have closed the idle connection on force-cancel";
}

// After shutdown the accept loop has been cancelled AND the acceptor closed, so a
// fresh connection is REFUSED (not silently backlogged). This is the only test
// that asserts the accept-loop's co_spawn-slot cancellation actually fired —
// without it the fixture's ioc_.stop() would mask a propagation failure (spike S4).
TEST_F(HttpDrainTest, NewConnectionsRefusedAfterShutdown) {
    graceful_shutdown(100ms);

    boost::asio::io_context cioc;
    boost::asio::ip::tcp::socket sock{cioc};
    boost::system::error_code ec;
    sock.connect({boost::asio::ip::make_address("127.0.0.1"), port_}, ec);
    EXPECT_TRUE(ec) << "the closed acceptor must refuse new connections after shutdown";
}
