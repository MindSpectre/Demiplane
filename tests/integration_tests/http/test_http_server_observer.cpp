#include <algorithm>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <boost/asio/steady_timer.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast/http/verb.hpp>
#include <gtest/gtest.h>
#include <http11_config.hpp>
#include <http11_driver.hpp>
#include <server.hpp>
#include <server_observer.hpp>

#include "server_test_fixture.hpp"

using namespace demiplane::http;
namespace bhttp = boost::beast::http;
using namespace std::chrono_literals;

namespace {

    /// Thread-safe event log — hooks fire on io threads, asserts run on the
    /// test thread.
    class RecordingObserver final : public ServerObserver {
    public:
        boost::asio::awaitable<void> on_setup_complete() override {
            push("setup_complete");
            co_return;
        }
        boost::asio::awaitable<void> on_shutdown_started() override {
            // REAL async work: if the Server failed to await this,
            // shutdown_complete would overtake it and the order assert fails.
            const auto ex = co_await boost::asio::this_coro::executor;
            boost::asio::steady_timer t{ex};
            t.expires_after(100ms);
            co_await t.async_wait(boost::asio::use_awaitable);
            push("shutdown_started");
        }
        void on_shutdown_complete() noexcept override {
            push("shutdown_complete");
        }
        void on_request(const RequestContext& ctx) noexcept override {
            push("request:" + std::string{ctx.target()});
        }
        void on_response(const RequestInfo& info, const Response& resp) noexcept override {
            push("response:" + std::string{info.target} + ":" + std::to_string(std::to_underlying(resp.status)));
        }
        void on_unhandled_exception([[maybe_unused]] const std::exception_ptr& ep) noexcept override {
            push("exception");
        }

        [[nodiscard]] std::vector<std::string> events() {
            std::lock_guard lk{mu_};
            return events_;
        }
        [[nodiscard]] bool saw(const std::string& e) {
            std::lock_guard lk{mu_};
            return std::ranges::find(events_, e) != events_.end();
        }
        [[nodiscard]] std::ptrdiff_t index_of(const std::string& e) {
            std::lock_guard lk{mu_};
            const auto it = std::ranges::find(events_, e);
            return it == events_.end() ? -1 : it - events_.begin();
        }

    private:
        void push(std::string e) {
            std::lock_guard lk{mu_};
            events_.push_back(std::move(e));
        }
        std::mutex mu_;
        std::vector<std::string> events_;
    };

    /// on_shutdown_started throws — must fan to on_unhandled_exception and
    /// NOT abort the shutdown.
    class ThrowingShutdownObserver final : public ServerObserver {
    public:
        boost::asio::awaitable<void> on_shutdown_started() override {
            throw std::runtime_error{"observer exploded"};
            co_return;  // unreachable; keeps this a coroutine
        }
    };

}  // namespace

class ServerObserverTest : public http_it::ServerIntegrationFixture {
protected:
    std::shared_ptr<RecordingObserver> observer_ = std::make_shared<RecordingObserver>();

    void start_with_observer() {
        start_server([&](Server& s) {
            s.add_observer(observer_);
            s.add_controller(std::make_shared<http_it::PingController>());
            s.add_tcp_listener("127.0.0.1", 0, Http11Driver{Http11Config{}});
        });
    }

    void wait_for(const std::string& event, const std::chrono::seconds cap = 2s) {
        const auto deadline = std::chrono::steady_clock::now() + cap;
        while (!observer_->saw(event) && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(1ms);
        }
        ASSERT_TRUE(observer_->saw(event)) << "timed out waiting for '" << event << "'";
    }
};

TEST_F(ServerObserverTest, LifecycleAndRequestHooksFireInOrder) {
    start_with_observer();
    wait_for("setup_complete");  // D4: notified async on the executor

    http_it::TcpClient client{port()};
    EXPECT_EQ(client.send(bhttp::verb::get, "/ping").result_int(), 200u);
    wait_for("response:/ping:200");

    server_->stop();
    server_->wait_until_stopped();

    // shutdown_started (with its 100ms of awaited async work) must precede
    // shutdown_complete, which must precede wait_until_stopped() returning —
    // both are already in the log HERE.
    const auto i_setup    = observer_->index_of("setup_complete");
    const auto i_req      = observer_->index_of("request:/ping");
    const auto i_res      = observer_->index_of("response:/ping:200");
    const auto i_started  = observer_->index_of("shutdown_started");
    const auto i_complete = observer_->index_of("shutdown_complete");
    ASSERT_NE(i_setup, -1);
    ASSERT_NE(i_req, -1);
    ASSERT_NE(i_res, -1);
    ASSERT_NE(i_started, -1);
    ASSERT_NE(i_complete, -1);
    EXPECT_LT(i_setup, i_req);
    EXPECT_LT(i_req, i_res);
    EXPECT_LT(i_res, i_started);
    EXPECT_LT(i_started, i_complete);
}

TEST_F(ServerObserverTest, ResponseHookFiresForRoutingMisses) {
    start_with_observer();
    http_it::TcpClient client{port()};
    EXPECT_EQ(client.send(bhttp::verb::get, "/nope").result_int(), 404u);
    wait_for("response:/nope:404");
    EXPECT_TRUE(observer_->saw("request:/nope"));
}

TEST_F(ServerObserverTest, UnhandledExceptionHookFiresAndClientGets500) {
    start_with_observer();
    http_it::TcpClient client{port()};
    EXPECT_EQ(client.send(bhttp::verb::get, "/boom").result_int(), 500u);
    wait_for("exception");
    EXPECT_TRUE(observer_->saw("request:/boom"));
    // The 500 is DRIVER-synthesized after the rethrow — invisible to
    // on_response (D2, documented).
    EXPECT_FALSE(observer_->saw("response:/boom:500"));
}

TEST_F(ServerObserverTest, ThrowingShutdownObserverFansToAllAndShutdownCompletes) {
    start_server([&](Server& s) {
        s.add_observer(std::make_shared<ThrowingShutdownObserver>());
        s.add_observer(observer_);
        s.add_controller(std::make_shared<http_it::PingController>());
        s.add_tcp_listener("127.0.0.1", 0, Http11Driver{Http11Config{}});
    });
    server_->stop();
    server_->wait_until_stopped();                     // completes despite the throw
    EXPECT_TRUE(observer_->saw("exception"));          // fanned to EVERY observer
    EXPECT_TRUE(observer_->saw("shutdown_complete"));  // phases 4-6 still ran
}
