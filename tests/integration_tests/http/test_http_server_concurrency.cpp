#include <atomic>
#include <cstddef>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <boost/beast/http/verb.hpp>
#include <gtest/gtest.h>

#include <controller.hpp>
#include <http11_config.hpp>
#include <http11_driver.hpp>
#include <request_context.hpp>
#include <server.hpp>

#include "server_test_fixture.hpp"

using namespace demiplane::http;
namespace bhttp = boost::beast::http;

namespace {

    class CountingController final : public HttpController {
    public:
        std::atomic<std::size_t> served{0};

        void configure_routes() override {
            Get("/ping", &CountingController::ping);
            Get("/users/{id}", &CountingController::user);
        }

    private:
        AsyncResponse ping(RequestContext ctx) {
            served.fetch_add(1, std::memory_order_relaxed);
            co_return ctx.ok("pong");
        }
        AsyncResponse user(RequestContext ctx) {
            served.fetch_add(1, std::memory_order_relaxed);
            co_return ctx.ok("user:" + std::to_string(ctx.path_param<int>("id").value_or(-1)));
        }
    };

}  // namespace

class ServerConcurrencyTest : public http_it::ServerIntegrationFixture {};

TEST_F(ServerConcurrencyTest, ThousandRequestsAcrossFourIoWorkers) {
    constexpr std::size_t CLIENTS    = 8;
    constexpr std::size_t PER_CLIENT = 125;  // 8 × 125 = 1000 (spec §14.2)

    auto ctrl = std::make_shared<CountingController>();
    start_server(
        [&](Server& s) {
            s.add_controller(ctrl);
            s.add_tcp_listener("127.0.0.1", 0, Http11Driver{Http11Config{}});
        },
        ServerConfig::Builder{}.finalize(),
        /*io_threads=*/4);

    std::atomic<std::size_t> failures{0};
    std::vector<std::thread> clients;
    clients.reserve(CLIENTS);
    for (std::size_t c = 0; c < CLIENTS; ++c) {
        clients.emplace_back([&, c] {
            try {
                http_it::TcpClient client{port()};  // one keep-alive socket per client thread
                for (std::size_t j = 0; j < PER_CLIENT; ++j) {
                    const bool keep = j + 1 < PER_CLIENT;  // final request closes the socket
                    if ((c + j) % 2 == 0) {
                        const auto res = client.send(bhttp::verb::get, "/ping", {}, "text/plain", keep);
                        if (res.result_int() != 200u || res.body() != "pong") {
                            failures.fetch_add(1, std::memory_order_relaxed);
                        }
                    } else {
                        const auto id  = std::to_string(c * PER_CLIENT + j);
                        const auto res = client.send(bhttp::verb::get, "/users/" + id, {}, "text/plain", keep);
                        if (res.result_int() != 200u || res.body() != "user:" + id) {
                            failures.fetch_add(1, std::memory_order_relaxed);
                        }
                    }
                }
            } catch (...) {
                failures.fetch_add(PER_CLIENT, std::memory_order_relaxed);
            }
        });
    }
    for (auto& t : clients) {
        t.join();
    }

    EXPECT_EQ(failures.load(), 0u);
    EXPECT_EQ(ctrl->served.load(), CLIENTS * PER_CLIENT);
    // TearDown runs the full graceful shutdown under the 4-worker executor —
    // the shutdown paths get the same TSan coverage as the serve path.
}
