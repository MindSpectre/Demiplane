#include <memory>
#include <string>

#include <boost/beast/http/verb.hpp>
#include <gtest/gtest.h>

#include <controller.hpp>
#include <request_context.hpp>

#include "http_test_fixture.hpp"

using namespace demiplane::http;
namespace bhttp = boost::beast::http;

namespace {

    // TODO(PR5): broaden §14.2 coverage — PUT/PATCH/HEAD/OPTIONS verbs, multiple path params, and URL-decoded path/query *values*.

    class EchoController final : public HttpController {
    public:
        void configure_routes() override {
            Get("/hello", &EchoController::hello);
            Get("/users/{id}", &EchoController::user);
            Post("/echo", &EchoController::echo);
            Get("/boom", &EchoController::boom);
        }

    private:
        AsyncResponse hello(RequestContext ctx) {
            co_return ctx.ok("hello world");
        }
        AsyncResponse user(RequestContext ctx) {
            co_return ctx.ok("user:" + std::to_string(ctx.path_param<int>("id").value_or(-1))
                             + " v=" + ctx.query_or<std::string>("v", "none"));
        }
        AsyncResponse echo(RequestContext ctx) {
            auto body = co_await ctx.body().read_to_string(1 << 20);
            if (!body) {
                co_return ctx.status(HttpStatus::payload_too_large, "too big");
            }
            co_return ctx.json(std::move(body).value());
        }
        AsyncResponse boom(RequestContext) {
            throw std::runtime_error{"handler exploded"};
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
    const std::string payload = R"({"name":"demiplane"})";
    auto res = client.send(bhttp::verb::post, "/echo", payload, "application/json");
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
    auto res = client.send(bhttp::verb::delete_, "/hello");
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
