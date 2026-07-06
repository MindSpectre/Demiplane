#include <chrono>
#include <future>
#include <memory>
#include <thread>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/post.hpp>
#include <boost/beast/http/verb.hpp>
#include <gtest/gtest.h>

#include <http11_config.hpp>
#include <http11_driver.hpp>
#include <route_registry.hpp>  // RouteConflictAggregateError
#include <server.hpp>

#include "server_test_fixture.hpp"

using namespace demiplane::http;
namespace bhttp = boost::beast::http;
using namespace std::chrono_literals;

class ServerLifecycleTest : public http_it::ServerIntegrationFixture {};

TEST_F(ServerLifecycleTest, ServesAndStopsGracefully) {
    start_server([](Server& s) {
        s.add_controller(std::make_shared<http_it::PingController>());
        s.add_tcp_listener("127.0.0.1", 0, Http11Driver{Http11Config{}});
    });
    EXPECT_TRUE(server_->is_running());
    ASSERT_GT(port(), 0);

    http_it::TcpClient client{port()};
    const auto res = client.send(bhttp::verb::get, "/ping");
    EXPECT_EQ(res.result_int(), 200u);
    EXPECT_EQ(res.body(), "pong");

    server_->stop();
    server_->wait_until_stopped();
    EXPECT_FALSE(server_->is_running());

    // §9.7: stop() must NOT stop the caller's executor — it may be shared
    // with subsystems that outlive HTTP. Prove it still executes work.
    std::promise<void> ran;
    boost::asio::post(ioc_, [&ran] { ran.set_value(); });
    ASSERT_EQ(ran.get_future().wait_for(1s), std::future_status::ready);
}

namespace {

    /// Registers GET /dup — two instances collide on freeze().
    class DupController final : public HttpController {
    public:
        void configure_routes() override {
            Get("/dup", &DupController::h);
        }

    private:
        AsyncResponse h(RequestContext ctx) {
            co_return ctx.ok("dup");
        }
    };

    void add_ping_tcp(Server& s) {
        s.add_controller(std::make_shared<http_it::PingController>());
        s.add_tcp_listener("127.0.0.1", 0, Http11Driver{Http11Config{}});
    }

}  // namespace

TEST_F(ServerLifecycleTest, StopBeforeSetupIsANoOp) {
    Server server{ServerConfig{}, ioc_.get_executor()};
    server.stop();  // documented no-op (spec §9.7 corollary)
    EXPECT_FALSE(server.is_running());
    server.wait_until_stopped();  // returns immediately — setup() never ran
}

TEST_F(ServerLifecycleTest, StopIsIdempotentAndWaitReturnsAgain) {
    start_server(add_ping_tcp);
    server_->stop();
    server_->stop();  // second call: CAS fails, silently ignored
    server_->wait_until_stopped();
    server_->stop();  // after stopped: still a no-op
    server_->wait_until_stopped();  // shutdown_complete_ latched — immediate
    EXPECT_FALSE(server_->is_running());
}

TEST_F(ServerLifecycleTest, SetupWithoutListenersThrows) {
    Server server{ServerConfig{}, ioc_.get_executor()};
    server.add_controller(std::make_shared<http_it::PingController>());
    EXPECT_THROW(server.setup(), std::logic_error);
    EXPECT_FALSE(server.is_running());
}

TEST_F(ServerLifecycleTest, RegistrationAfterSetupThrows) {
    start_server(add_ping_tcp);
    EXPECT_THROW(server_->add_tcp_listener("127.0.0.1", 0, Http11Driver{Http11Config{}}), std::logic_error);
    EXPECT_THROW(server_->add_observer(std::make_shared<ServerObserver>()), std::logic_error);
    EXPECT_THROW((void)server_->in_group("/late"), std::logic_error);  // (void): in_group is [[nodiscard]]
    EXPECT_THROW(server_->add_controller(std::make_shared<http_it::PingController>()), std::logic_error);
    EXPECT_THROW(server_->setup(), std::logic_error);  // double setup
    EXPECT_TRUE(server_->is_running());                // still serving despite the throws
}

TEST_F(ServerLifecycleTest, SetupThrowsWhenPortInUse) {
    // §14.2 lifecycle: setup() failure on port-in-use (resolves the PR 4
    // port-in-use marker in test_tcp_listener.cpp). SO_REUSEADDR does not permit two
    // LISTENING sockets on one port — the second bind is EADDRINUSE.
    boost::asio::io_context probe;
    const boost::asio::ip::tcp::acceptor taken{
        probe, {boost::asio::ip::make_address("127.0.0.1"), 0}};  // open+bind+listen
    const auto port = taken.local_endpoint().port();

    server_.emplace(ServerConfig{}, ioc_.get_executor());
    server_->add_controller(std::make_shared<http_it::PingController>());
    server_->add_tcp_listener("127.0.0.1", port, Http11Driver{Http11Config{}});
    // Bind failures are collected best-effort-all and aggregated; acceptors
    // that DID bind are released (listener set cleared) before the throw.
    EXPECT_THROW(server_->setup(), ListenerBindError);
    EXPECT_FALSE(server_->is_running());
    EXPECT_TRUE(server_->listeners().empty());
}

TEST_F(ServerLifecycleTest, ConflictingRoutesThrowAggregateAtSetup) {
    server_.emplace(ServerConfig{}, ioc_.get_executor());
    server_->add_controller(std::make_shared<DupController>());
    server_->add_controller(std::make_shared<DupController>());
    server_->add_tcp_listener("127.0.0.1", 0, Http11Driver{Http11Config{}});
    try {
        server_->setup();
        FAIL() << "expected RouteConflictAggregateError";
    } catch (const RouteConflictAggregateError& e) {
        EXPECT_EQ(e.conflicts().size(), 1u);
        EXPECT_NE(std::string{e.what()}.find("/dup"), std::string::npos);
    }
    EXPECT_FALSE(server_->is_running());
}

TEST_F(ServerLifecycleTest, GracefulShutdownCompletesInFlightRequests) {
    auto latch = std::make_shared<http_it::LatchController>();
    start_server([&](Server& s) {
        s.add_controller(latch);
        s.add_tcp_listener("127.0.0.1", 0, Http11Driver{Http11Config{}});
    });

    http_it::TcpClient client{port()};
    client.write_request(bhttp::verb::get, "/slow");
    while (!latch->slow_entered.load(std::memory_order_acquire)) {
        std::this_thread::sleep_for(1ms);  // request provably in flight
    }
    server_->stop();
    server_->wait_until_stopped();

    // Drain phase let the 150ms handler finish and the response reach the
    // wire BEFORE shutdown completed (spec §14.2 "in-flight completes").
    const auto res = client.read_response();
    EXPECT_EQ(res.result_int(), 200u);
    EXPECT_EQ(res.body(), "slow done");
}

TEST_F(ServerLifecycleTest, NewConnectionsRefusedAfterShutdown) {
    start_server(add_ping_tcp);
    const auto p = port();
    server_->stop();
    server_->wait_until_stopped();
    // Acceptors are closed in phase 1.5 — a fresh connect is REFUSED, not
    // silently backlogged (spec §14.2; the TcpClient ctor connect throws).
    EXPECT_THROW(http_it::TcpClient{p}, boost::system::system_error);
}

TEST_F(ServerLifecycleTest, DrainDeadlineForceCancelsStragglers) {
    ServerConfig cfg;
    cfg.drain_timeout = std::chrono::milliseconds{100};  // << the 500ms /hang handler
    auto latch        = std::make_shared<http_it::LatchController>();
    start_server(
        [&](Server& s) {
            s.add_controller(latch);
            s.add_tcp_listener("127.0.0.1", 0, Http11Driver{Http11Config{}});
        },
        cfg);

    http_it::TcpClient client{port()};
    client.write_request(bhttp::verb::get, "/hang");
    while (!latch->hang_entered.load(std::memory_order_acquire)) {
        std::this_thread::sleep_for(1ms);
    }
    const auto t0 = std::chrono::steady_clock::now();
    server_->stop();
    server_->wait_until_stopped();
    // Bounded: 100ms drain + force-cancel + handler-completion unwind (the
    // 500ms timer is not slot-bound — phase 2.5 waits for it), NOT the
    // 30s default drain budget.
    EXPECT_LT(std::chrono::steady_clock::now() - t0, 5s);

    // Force-cancelled connection: socket closed, no response ever written.
    const auto ec = client.read_after_close();
    EXPECT_TRUE(ec) << "expected the force-cancelled connection to be dead, got a clean read";
}

TEST_F(ServerLifecycleTest, AsyncWaitStoppedCompletesWithShutdown) {
    start_server(add_ping_tcp);
    // The awaitable twin (§9.1) — for callers already ON the executor, where
    // the blocking wait_until_stopped() would deadlock the shutdown it awaits.
    std::promise<void> stopped;
    boost::asio::co_spawn(
        ioc_,
        [this]() -> boost::asio::awaitable<void> { co_await server_->async_wait_stopped(); },
        [&stopped](std::exception_ptr) { stopped.set_value(); });
    auto fut = stopped.get_future();
    EXPECT_EQ(fut.wait_for(50ms), std::future_status::timeout);  // still running — must not complete early
    server_->stop();
    ASSERT_EQ(fut.wait_for(5s), std::future_status::ready);  // completes once shutdown finishes
    server_->wait_until_stopped();                            // latched — returns immediately
    EXPECT_FALSE(server_->is_running());
}

TEST_F(ServerLifecycleTest, GroupPrefixMountsControllerAtServerLevel) {
    start_server([](Server& s) {
        s.in_group("/api/v1").add_controller(std::make_shared<http_it::PingController>());
        s.add_tcp_listener("127.0.0.1", 0, Http11Driver{Http11Config{}});
    });
    http_it::TcpClient client{port()};
    // keep_alive=true: the second request reuses the socket.
    EXPECT_EQ(client.send(bhttp::verb::get, "/api/v1/ping", {}, "text/plain", true).body(), "pong");
    EXPECT_EQ(client.send(bhttp::verb::get, "/ping").result_int(), 404u);  // unprefixed path not mounted
}
