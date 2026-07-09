#include <atomic>
#include <chrono>
#include <csignal>
#include <memory>
#include <stdexcept>
#include <thread>

#include <boost/beast/http/verb.hpp>
#include <gtest/gtest.h>
#include <unistd.h>

#include <http11_config.hpp>
#include <http11_driver.hpp>
#include <run_standalone.hpp>
#include <server.hpp>

#include "server_test_fixture.hpp"

using namespace demiplane::http;
namespace bhttp = boost::beast::http;
using namespace std::chrono_literals;

namespace {

    /// GET /stop → 204 + server.stop() — programmatic shutdown from ON the
    /// executor, the only safe placement for a run_standalone-owned Server:
    /// an EXTERNAL thread's stop() posts into the internal io_context and its
    /// scheduler-signal tail races the io_context destruction that follows
    /// shutdown completion (TSan-verified: pthread_cond_signal in stop()'s
    /// co_spawn vs pthread_cond_destroy in ~io_context).
    class StopController final : public HttpController {
    public:
        explicit StopController(std::atomic<Server*>& server)
            : server_{&server} {
        }
        void configure_routes() override {
            Get("/stop", [srv = server_](RequestContext ctx) -> AsyncResponse {
                if (auto* s = srv->load(std::memory_order_acquire)) {
                    s->stop();  // executor thread — documented-safe; this request still drains
                }
                co_return ctx.status(HttpStatus::no_content);
            });
        }

    private:
        std::atomic<Server*>* server_;
    };

    /// Drives run_standalone on a side thread; the test thread plays "ops".
    struct StandaloneRun {
        std::atomic<Server*> server{nullptr};
        std::atomic<bool> returned{false};
        std::thread runner;

        explicit StandaloneRun(const std::size_t threads = 2, const bool with_stop_route = false) {
            runner = std::thread{[this, threads, with_stop_route] {
                run_standalone(ServerConfig::Builder{}.finalize(), threads, [this, with_stop_route](Server& s) {
                    server.store(&s, std::memory_order_release);  // valid until run_standalone returns
                    s.add_controller(std::make_shared<http_it::PingController>());
                    if (with_stop_route) {
                        s.add_controller(std::make_shared<StopController>(server));
                    }
                    s.add_tcp_listener("127.0.0.1", 0, Http11Driver{Http11Config{}});
                });
                returned.store(true, std::memory_order_release);
            }};
        }

        /// Wait until the server is live; returns its bound port.
        std::uint16_t await_live() {
            const auto deadline = std::chrono::steady_clock::now() + 5s;
            while (std::chrono::steady_clock::now() < deadline) {
                if (auto* s = server.load(std::memory_order_acquire); s && s->is_running()) {
                    return s->listeners().front()->bound_port();
                }
                std::this_thread::sleep_for(1ms);
            }
            ADD_FAILURE() << "run_standalone never went live";
            return 0;
        }

        void await_return() {
            const auto deadline = std::chrono::steady_clock::now() + 5s;
            while (!returned.load(std::memory_order_acquire) && std::chrono::steady_clock::now() < deadline) {
                std::this_thread::sleep_for(1ms);
            }
            EXPECT_TRUE(returned.load(std::memory_order_acquire)) << "run_standalone did not return";
            if (runner.joinable()) {
                runner.join();
            }
        }

        ~StandaloneRun() {
            if (runner.joinable()) {
                runner.join();
            }
        }
    };

}  // namespace

TEST(RunStandaloneTest, ServesAndStopsOnSigint) {
    StandaloneRun run;
    const auto port = run.await_live();
    ASSERT_GT(port, 0);
    {
        http_it::TcpClient client{port};
        const auto res = client.send(bhttp::verb::get, "/ping");
        EXPECT_EQ(res.result_int(), 200u);
        EXPECT_EQ(res.body(), "pong");
    }
    ::kill(::getpid(), SIGINT);  // §14.2: SIGINT → graceful shutdown
    run.await_return();
    // run.server is dangling once returned — do not touch it here.
}

TEST(RunStandaloneTest, StopsViaHandlerStop) {
    StandaloneRun run{2, /*with_stop_route=*/true};
    const auto port = run.await_live();
    ASSERT_GT(port, 0);
    {
        // Programmatic path, no signal: the handler calls server.stop() ON the
        // executor; the in-flight /stop request itself drains to completion.
        http_it::TcpClient client{port};
        EXPECT_EQ(client.send(bhttp::verb::get, "/stop").result_int(), 204u);
    }
    run.await_return();
}

TEST(RunStandaloneTest, ZeroThreadsThrows) {
    EXPECT_THROW(run_standalone(ServerConfig::Builder{}.finalize(), 0, [](Server&) {}), std::invalid_argument);
}
