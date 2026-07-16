#include <atomic>
#include <chrono>
#include <memory>
#include <thread>

#include <boost/asio/steady_timer.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast/http/verb.hpp>
#include <controller.hpp>
#include <gtest/gtest.h>
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
            Get("/very-slow", &DrainController::very_slow);
        }

        // Entry latch: lets a test deterministically wait until a request is
        // INSIDE router.dispatch (the window where the conn's cancel slot has no
        // installed handler) before it triggers the drain deadline.
        std::atomic<bool> very_slow_entered{false};

    private:
        static AsyncResponse hello(RequestContext ctx) {
            co_return ctx.ok("hello world");
        }
        static AsyncResponse slow(RequestContext ctx) {
            const auto ex = co_await boost::asio::this_coro::executor;
            boost::asio::steady_timer t{ex};
            t.expires_after(150ms);
            co_await t.async_wait(use_strand_awaitable);
            co_return ctx.ok("slow done");
        }
        AsyncResponse very_slow(RequestContext ctx) {
            very_slow_entered.store(true, std::memory_order_release);
            const auto ex = co_await boost::asio::this_coro::executor;
            boost::asio::steady_timer t{ex};
            t.expires_after(400ms);
            co_await t.async_wait(use_strand_awaitable);
            co_return ctx.ok("slow done");
        }
    };

    class HttpDrainTest : public http_it::HttpIntegrationFixture {
    protected:
        std::shared_ptr<DrainController> controller_;

        void SetUp() override {
            controller_ = std::make_shared<DrainController>();
            add_controller(controller_);
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
        status   = static_cast<int>(res.result_int());
        body     = res.body();
    }};

    std::this_thread::sleep_for(50ms);  // let the request reach the slow handler
    graceful_shutdown(2s);              // drain must wait for the in-flight /slow
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

    graceful_shutdown(100ms);  // short drain → force-cancel the idle conn

    const auto ec = client.read_after_close();
    EXPECT_TRUE(ec) << "server should have closed the idle connection on force-cancel";
}

// A connection whose handler is RUNNING at the drain deadline (not parked in
// slot-bound driver I/O) must still be force-cancelled. Edge- vs level-trigger
// regression test: cancellation_signal::emit alone is LOST in this window (the
// driver binds the conn slot per-op; router.dispatch is unbound), so cancel()
// must also close the socket — the kill then lands at the handler's next I/O.
TEST_F(HttpDrainTest, BusyHandlerConnectionKilledAtDrainDeadline) {
    http_it::TcpClient client{port_};
    client.write_request(bhttp::verb::get, "/very-slow");

    // Deterministically reach the un-slot-bound dispatch window (a blind sleep
    // could fire the drain while the request is still parked in read_header,
    // where the plain emit works and the test would vacuously pass).
    const auto entry_cap = std::chrono::steady_clock::now() + 2s;
    while (!controller_->very_slow_entered.load(std::memory_order_acquire) &&
           std::chrono::steady_clock::now() < entry_cap) {
        std::this_thread::sleep_for(1ms);
    }
    ASSERT_TRUE(controller_->very_slow_entered.load()) << "request never reached the handler";

    graceful_shutdown(100ms);  // deadline fires with ~300ms of handler still to run

    const auto ec = client.read_after_close();
    EXPECT_TRUE(ec) << "busy connection must be killed at the drain deadline, got a full response instead";
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
