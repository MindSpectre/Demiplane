#include <chrono>
#include <deque>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/beast/http/verb.hpp>
#include <controller.hpp>
#include <gtest/gtest.h>
#include <request_context.hpp>

#include "http_test_fixture.hpp"

using namespace demiplane::http;
namespace bhttp = boost::beast::http;

namespace {

    class EchoController final : public HttpController {
    public:
        void configure_routes() override {
            Get("/hello", &EchoController::hello);
            Get("/users/{id}", &EchoController::user);
            Post("/echo", &EchoController::echo);
            Get("/boom", &EchoController::boom);
            Put("/items/{id}", &EchoController::put_item);
            Patch("/items/{id}", &EchoController::patch_item);
            Head("/hello", &EchoController::hello_head);
            Options("/hello", &EchoController::hello_options);
            Get("/users/{id}/posts/{post_id}", &EchoController::user_post);
            Get("/files/{name}", &EchoController::file_name);
            Get("/search", &EchoController::search);
        }

    private:
        static AsyncResponse hello(RequestContext ctx) {
            co_return ctx.ok("hello world");
        }
        static AsyncResponse user(RequestContext ctx) {
            co_return ctx.ok("user:" + std::to_string(ctx.path_param<int>("id").value_or(-1)) +
                             " v=" + ctx.query_or<std::string>("v", "none"));
        }
        static AsyncResponse echo(RequestContext ctx) {
            auto body = co_await ctx.body().read_to_string(1 << 20);
            if (!body) {
                co_return ctx.status(HttpStatus::payload_too_large, "too big");
            }
            co_return ctx.json(std::move(body).value());
        }
        static AsyncResponse boom(RequestContext) {
            throw std::runtime_error{"handler exploded"};
        }
        static AsyncResponse put_item(RequestContext ctx) {
            auto body = co_await ctx.body().read_to_string(1 << 20);
            co_return ctx.ok("put:" + ctx.path_param<std::string>("id").value_or("?") + ":" + std::move(body).value());
        }
        static AsyncResponse patch_item(RequestContext ctx) {
            auto body = co_await ctx.body().read_to_string(1 << 20);
            co_return ctx.ok("patch:" + ctx.path_param<std::string>("id").value_or("?") + ":" +
                             std::move(body).value());
        }
        static AsyncResponse hello_head(RequestContext ctx) {
            // Empty body — a HEAD response must not carry a payload.
            co_return ctx.status(HttpStatus::ok).set_header("X-Head-Route", "yes");
        }
        static AsyncResponse hello_options(RequestContext ctx) {
            co_return ctx.status(HttpStatus::no_content).set_header("Allow", "GET, HEAD, OPTIONS");
        }
        static AsyncResponse user_post(RequestContext ctx) {
            co_return ctx.ok(ctx.path_param<std::string>("id").value_or("?") + "|" +
                             ctx.path_param<std::string>("post_id").value_or("?"));
        }
        static AsyncResponse file_name(RequestContext ctx) {
            co_return ctx.ok("file:" + ctx.path_param<std::string>("name").value_or("?"));
        }
        static AsyncResponse search(RequestContext ctx) {
            co_return ctx.ok("q=" + ctx.query_or<std::string>("q", "none"));
        }
    };

    class HttpTcpTest : public http_it::HttpIntegrationFixture {
    protected:
        void SetUp() override {
            add_controller(std::make_shared<EchoController>());
            start(make_tcp_listener());
        }
    };

}  // namespace

TEST_F(HttpTcpTest, GetRoundTrip) {
    http_it::TcpClient client{port_};
    auto res = client.send(bhttp::verb::get, "/hello");
    EXPECT_EQ(res.result_int(), 200u);
    EXPECT_EQ(res.body(), "hello world");
    EXPECT_EQ(std::string(res[bhttp::field::server]), "Demiplane");
}

TEST_F(HttpTcpTest, PathAndQueryParams) {
    http_it::TcpClient client{port_};
    auto res = client.send(bhttp::verb::get, "/users/42?v=hi");
    EXPECT_EQ(res.result_int(), 200u);
    EXPECT_EQ(res.body(), "user:42 v=hi");
}

TEST_F(HttpTcpTest, PostJsonEchoedThroughArena) {
    http_it::TcpClient client{port_};
    constexpr std::string payload = R"({"name":"demiplane"})";
    auto res                      = client.send(bhttp::verb::post, "/echo", payload, "application/json");
    EXPECT_EQ(res.result_int(), 200u);
    EXPECT_EQ(res.body(), payload);
    EXPECT_EQ(std::string(res[bhttp::field::content_type]), "application/json");
}

TEST_F(HttpTcpTest, UnknownPathIs404) {
    http_it::TcpClient client{port_};
    EXPECT_EQ(client.send(bhttp::verb::get, "/nope").result_int(), 404u);
}

TEST_F(HttpTcpTest, WrongVerbIs405WithAllow) {
    http_it::TcpClient client{port_};
    const auto res = client.send(bhttp::verb::delete_, "/hello");
    EXPECT_EQ(res.result_int(), 405u);
    EXPECT_NE(std::string(res["Allow"]).find("GET"), std::string::npos);
}

TEST_F(HttpTcpTest, HandlerExceptionBecomes500) {
    http_it::TcpClient client{port_};
    // A 500 RESPONSE, not a dropped connection (the original module's bug).
    EXPECT_EQ(client.send(bhttp::verb::get, "/boom").result_int(), 500u);
}

TEST_F(HttpTcpTest, KeepAliveServesTwoRequestsOnOneSocket) {
    http_it::TcpClient client{port_};
    auto first = client.send(bhttp::verb::get, "/hello", {}, "text/plain", /*keep_alive=*/true);
    EXPECT_EQ(first.body(), "hello world");
    EXPECT_TRUE(first.keep_alive());
    auto second = client.send(bhttp::verb::get, "/users/7?v=q", {}, "text/plain", /*keep_alive=*/false);
    EXPECT_EQ(second.body(), "user:7 v=q");
}

TEST_F(HttpTcpTest, PutRoundTripsBodyAndParam) {
    http_it::TcpClient client{port_};
    const auto res = client.send(bhttp::verb::put, "/items/7", "new-name");
    EXPECT_EQ(res.result_int(), 200u);
    EXPECT_EQ(res.body(), "put:7:new-name");
}

TEST_F(HttpTcpTest, PatchRoundTripsBodyAndParam) {
    http_it::TcpClient client{port_};
    const auto res = client.send(bhttp::verb::patch, "/items/9", "delta");
    EXPECT_EQ(res.result_int(), 200u);
    EXPECT_EQ(res.body(), "patch:9:delta");
}

TEST_F(HttpTcpTest, HeadDispatchesHeadersWithoutBody) {
    http_it::TcpClient client{port_};
    const auto res = client.send_head("/hello");
    EXPECT_EQ(res.result_int(), 200u);
    EXPECT_EQ(std::string(res["X-Head-Route"]), "yes");
    EXPECT_TRUE(res.body().empty());
}

TEST_F(HttpTcpTest, OptionsReturnsAllow) {
    http_it::TcpClient client{port_};
    const auto res = client.send(bhttp::verb::options, "/hello");
    EXPECT_EQ(res.result_int(), 204u);
    EXPECT_NE(std::string(res["Allow"]).find("GET"), std::string::npos);
}

TEST_F(HttpTcpTest, MultiplePathParamsCapture) {
    http_it::TcpClient client{port_};
    const auto res = client.send(bhttp::verb::get, "/users/42/posts/777");
    EXPECT_EQ(res.result_int(), 200u);
    EXPECT_EQ(res.body(), "42|777");
}

TEST_F(HttpTcpTest, PathParamValuesUrlDecode) {
    // %20 decodes to space; a raw '+' in a PATH stays literal
    // (plus_is_space=false — spec §8.6).
    http_it::TcpClient client{port_};
    const auto res = client.send(bhttp::verb::get, "/files/a%20b+c");
    EXPECT_EQ(res.result_int(), 200u);
    EXPECT_EQ(res.body(), "file:a b+c");
}

TEST_F(HttpTcpTest, QueryParamValuesUrlDecodePlusAsSpace) {
    // In a QUERY both %20 and '+' decode to space (plus_is_space=true).
    http_it::TcpClient client{port_};
    const auto res = client.send(bhttp::verb::get, "/search?q=a%20b+c");
    EXPECT_EQ(res.result_int(), 200u);
    EXPECT_EQ(res.body(), "q=a b c");
}

// ── Round-robin placement across io_contexts (Finding 13) ────────────────────
// 2 contexts, only ctx0 driven at first: connections 0,2 land on ctx0 (served
// immediately), 1,3 land on the parked ctx1 (nothing until it runs). Proves
// the listener really distributes strands across the executor set.
TEST(TcpListenerDistribution, RoundRobinPlacesConnectionsAcrossContexts) {
    namespace asio = boost::asio;
    using namespace std::chrono_literals;

    std::deque<asio::io_context> ctxs;
    auto& ctx0 = ctxs.emplace_back();
    auto& ctx1 = ctxs.emplace_back();
    auto g0    = asio::make_work_guard(ctx0);
    auto g1    = asio::make_work_guard(ctx1);

    RouteRegistry registry;
    std::vector<std::shared_ptr<HttpController>> controllers;
    GroupBinding{registry, controllers, ""}.add_controller(std::make_shared<EchoController>());
    ASSERT_TRUE(registry.freeze().empty());
    Router router{registry};

    TcpListener<Http11Driver> listener{
        std::vector{ctx0.get_executor(), ctx1.get_executor()},
        "127.0.0.1", 0, Http11Driver{Http11Config{}}
    };
    listener.bind();
    asio::cancellation_signal stop;
    auto run_done =
        asio::co_spawn(ctx0, listener.run(router), asio::bind_cancellation_slot(stop.slot(), asio::use_future));

    std::thread w0{[&ctx0] { ctx0.run(); }};

    // Sequential connects ⇒ deterministic accept order ⇒ deterministic
    // round-robin placement (the accept loop is serialized).
    std::vector<std::unique_ptr<http_it::TcpClient>> clients;
    for (int i = 0; i < 4; ++i) {
        clients.push_back(std::make_unique<http_it::TcpClient>(listener.bound_port()));
        clients.back()->write_request(bhttp::verb::get, "/hello");
    }

    // ctx0's connections respond while ctx1 is parked...
    EXPECT_EQ(clients[0]->read_response().body(), "hello world");
    EXPECT_EQ(clients[2]->read_response().body(), "hello world");
    std::this_thread::sleep_for(50ms);
    // ...and ctx1's have not been served at all (their serve coroutines are
    // queued on the undriven context).
    EXPECT_EQ(clients[1]->available(), 0u);
    EXPECT_EQ(clients[3]->available(), 0u);

    // Drive ctx1: the parked connections serve normally.
    std::thread w1{[&ctx1] { ctx1.run(); }};
    EXPECT_EQ(clients[1]->read_response().body(), "hello world");
    EXPECT_EQ(clients[3]->read_response().body(), "hello world");

    // Teardown: stop accepting (emit serialized with the loop via ctx0),
    // then stop both contexts and join.
    asio::dispatch(ctx0, [&stop] { stop.emit(asio::cancellation_type::terminal); });
    run_done.wait();
    clients.clear();
    g0.reset();
    g1.reset();
    ctx0.stop();
    ctx1.stop();
    w0.join();
    w1.join();
}
