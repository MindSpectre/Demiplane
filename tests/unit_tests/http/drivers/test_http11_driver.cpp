#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include <controller.hpp>
#include <group.hpp>
#include <gtest/gtest.h>
#include <http11_config.hpp>
#include <http11_driver.hpp>
#include <route_registry.hpp>
#include <router.hpp>

#include "driver_test_utils.hpp"

using namespace demiplane::http;
using http_driver_test::exchange;
using http_driver_test::ParsedResponse;

namespace {
    // NOLINTBEGIN
    class ApiController final : public HttpController {
    public:
        void configure_routes() override {
            Get("/hello", &ApiController::hello);
            Get("/users/{id}", &ApiController::user);
            Post("/echo", &ApiController::echo);
            Get("/boom", &ApiController::boom);
        }

    private:
        AsyncResponse hello(RequestContext ctx) {
            co_return ctx.ok("hello world");
        }
        AsyncResponse user(RequestContext ctx) {
            co_return ctx.ok("user:" + std::to_string(ctx.path_param<int>("id").value_or(-1)) +
                             " v=" + ctx.query_or<std::string>("v", "none"));
        }
        AsyncResponse echo(RequestContext ctx) {
            auto body = co_await ctx.body().read_to_string(1 << 20);
            if (!body)  // gears::Outcome: explicit operator bool
                co_return ctx.status(HttpStatus::payload_too_large, "too big");
            co_return ctx.json(std::move(body).value());  // .value(), not operator*
        }
        AsyncResponse boom(RequestContext) {
            throw std::runtime_error{"handler exploded"};
        }
    };

    Http11Driver make_driver() {
        return Http11Driver{Http11Config{}};
    }
    // NOLINTEND
}  // namespace

class Http11DriverTest : public ::testing::Test {
protected:
    RouteRegistry registry_;
    std::vector<std::shared_ptr<HttpController>> controllers_;

    void SetUp() override {
        GroupBinding{registry_, controllers_, ""}.add_controller(std::make_shared<ApiController>());
        ASSERT_TRUE(registry_.freeze().empty());
    }

    Router router() {
        return Router{registry_};
    }
};

TEST_F(Http11DriverTest, GetRoundTrip) {
    auto driver = make_driver();
    auto r      = router();
    auto out    = exchange(driver, r, {"GET /hello HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n"}, 1);
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0].result_int(), 200u);
    EXPECT_EQ(out[0].body(), "hello world");
}

// ── Batch A — request body + path/query params ──────────────────────────────

TEST_F(Http11DriverTest, PathAndQueryParams) {
    auto driver = make_driver();
    auto r      = router();
    auto out    = exchange(driver, r, {"GET /users/42?v=hi HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n"}, 1);
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0].result_int(), 200u);
    EXPECT_EQ(out[0].body(), "user:42 v=hi");
}

TEST_F(Http11DriverTest, PostBodyEchoedThroughArena) {
    auto driver               = make_driver();
    auto r                    = router();
    const std::string payload = R"({"name":"demiplane"})";
    std::string req           = "POST /echo HTTP/1.1\r\nHost: x\r\nContent-Type: application/json\r\n"
                                "Content-Length: " +
                      std::to_string(payload.size()) + "\r\nConnection: close\r\n\r\n" + payload;
    auto out = exchange(driver, r, {req}, 1);
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0].result_int(), 200u);
    EXPECT_EQ(std::string(out[0][boost::beast::http::field::content_type]), "application/json");
    EXPECT_EQ(out[0].body(), payload);
}

TEST_F(Http11DriverTest, LargeBodyRoundTrips) {
    auto driver = make_driver();
    auto r      = router();
    const std::string payload(4096, 'Z');  // well past SSO — exercises the arena body block
    std::string req = "POST /echo HTTP/1.1\r\nHost: x\r\nContent-Length: " + std::to_string(payload.size()) +
                      "\r\nConnection: close\r\n\r\n" + payload;
    auto out = exchange(driver, r, {req}, 1);
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0].body(), payload);
}

// ── Batch B — keep-alive loop + connection close ─────────────────────────────

TEST_F(Http11DriverTest, KeepAliveServesTwoRequests) {
    auto driver = make_driver();
    auto r      = router();
    auto out    = exchange(driver,
                        r,
                           {"GET /hello HTTP/1.1\r\nHost: x\r\n\r\n",  // keep-alive (HTTP/1.1 default)
                            "GET /users/7?v=q HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n"},
                        2);
    ASSERT_EQ(out.size(), 2u);
    EXPECT_EQ(out[0].body(), "hello world");
    EXPECT_EQ(out[1].body(), "user:7 v=q");
}

TEST_F(Http11DriverTest, ConnectionCloseStopsAfterOne) {
    auto driver = make_driver();
    auto r      = router();
    auto out    = exchange(driver, r, {"GET /hello HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n"}, 2);
    // Only one response arrives; the driver closed after it (Connection: close).
    ASSERT_EQ(out.size(), 1u);
    EXPECT_FALSE(out[0].keep_alive());
}

// ── Batch C — 404/405, handler exception → 500, body limit → 413, header limit → 400 ──

TEST_F(Http11DriverTest, UnknownPathIs404) {
    auto driver = make_driver();
    auto r      = router();
    auto out    = exchange(driver, r, {"GET /nope HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n"}, 1);
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0].result_int(), 404u);
}

TEST_F(Http11DriverTest, WrongVerbIs405WithAllow) {
    auto driver = make_driver();
    auto r      = router();
    auto out    = exchange(driver, r, {"DELETE /hello HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n"}, 1);
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0].result_int(), 405u);
    EXPECT_NE(std::string(out[0]["Allow"]).find("GET"), std::string::npos);
}

TEST_F(Http11DriverTest, HandlerExceptionBecomes500) {
    auto driver = make_driver();
    auto r      = router();
    auto out    = exchange(driver, r, {"GET /boom HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n"}, 1);
    ASSERT_EQ(out.size(), 1u);  // a RESPONSE, not a dropped connection (the original bug)
    EXPECT_EQ(out[0].result_int(), 500u);
}

TEST_F(Http11DriverTest, OversizeBodyIs413) {
    Http11Config cfg;
    cfg.max_body_bytes = 16;  // tiny limit
    Http11Driver driver{cfg};
    auto r = router();
    const std::string payload(64, 'A');  // exceeds 16
    std::string req = "POST /echo HTTP/1.1\r\nHost: x\r\nContent-Length: " + std::to_string(payload.size()) +
                      "\r\nConnection: close\r\n\r\n" + payload;
    auto out = exchange(driver, r, {req}, 1);
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0].result_int(), 413u);
}

TEST_F(Http11DriverTest, OversizeHeadersIs400) {
    Http11Config cfg;
    cfg.max_header_bytes = 64;  // tiny limit
    Http11Driver driver{cfg};
    auto r = router();
    std::string req =
        "GET /hello HTTP/1.1\r\nHost: x\r\nX-Big: " + std::string(256, 'h') + "\r\nConnection: close\r\n\r\n";
    auto out = exchange(driver, r, {req}, 1);
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0].result_int(), 400u);
}

// ── Batch E — malformed request → 400 ────────────────────────────────────────

TEST_F(Http11DriverTest, MalformedRequestIs400) {
    auto driver = make_driver();
    auto r      = router();
    // A header line with no colon is a structurally malformed-but-framed request:
    // the stream is still writable, so the driver must answer 400, not drop it.
    auto out =
        exchange(driver, r, {"GET /hello HTTP/1.1\r\nHost: x\r\nNoColonHeaderLine\r\nConnection: close\r\n\r\n"}, 1);
    ASSERT_EQ(out.size(), 1u);  // a RESPONSE, not a silently dropped connection
    EXPECT_EQ(out[0].result_int(), 400u);
}

// ── Batch D — Date / Server stamping ─────────────────────────────────────────

TEST_F(Http11DriverTest, DateAndServerStamped) {
    auto driver = make_driver();
    auto r      = router();
    auto out    = exchange(driver, r, {"GET /hello HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n"}, 1);
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(std::string(out[0][boost::beast::http::field::server]), "Demiplane");
    const std::string date{out[0][boost::beast::http::field::date]};
    EXPECT_NE(date.find("GMT"), std::string::npos);
    // exactly one of each
    EXPECT_EQ(std::distance(out[0].equal_range("Date").first, out[0].equal_range("Date").second), 1);
    EXPECT_EQ(std::distance(out[0].equal_range("Server").first, out[0].equal_range("Server").second), 1);
}
