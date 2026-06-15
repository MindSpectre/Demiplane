#include <array>
#include <memory_resource>
#include <string>
#include <tuple>

#include <boost/beast/http.hpp>
#include <gtest/gtest.h>
#include <http11_driver.hpp>
#include <response.hpp>
#include <response_factory.hpp>

using namespace demiplane::http;
namespace http = boost::beast::http;

namespace {
    // Build a parsed Beast request of the driver's exact type, body in `arena`.
    detail::Http11Request make_parsed(std::pmr::polymorphic_allocator<> arena,
                                      http::verb verb,
                                      const std::string& target,
                                      const std::string& body) {
        std::pmr::polymorphic_allocator<char> body_alloc{arena.resource()};
        detail::Http11Request req{std::piecewise_construct, std::forward_as_tuple(body_alloc), std::forward_as_tuple()};
        req.method(verb);
        req.target(target);
        req.version(11);
        req.body().assign(body.begin(), body.end());
        req.set(http::field::content_type, "application/json");
        return req;
    }
}  // namespace

TEST(DriverHelpersTest, BuildRequestContextMapsMethodTargetVersionBody) {
    std::array<std::byte, 8192> block{};
    std::pmr::monotonic_buffer_resource res{block.data(), block.size()};
    std::pmr::polymorphic_allocator<> arena{&res};

    detail::Http11Request req = make_parsed(arena, http::verb::post, "/users/42?q=foo", "hello");
    RequestContext ctx        = detail::build_request_context(req, arena);

    EXPECT_EQ(ctx.method(), HttpMethod::post);
    EXPECT_EQ(ctx.target(), "/users/42?q=foo");
    EXPECT_EQ(ctx.path(), "/users/42");
    EXPECT_EQ(ctx.version(), HttpVersion::http_1_1);
    ASSERT_TRUE(ctx.body().buffered_view().has_value());
    EXPECT_EQ(*ctx.body().buffered_view(), "hello");
    EXPECT_TRUE(ctx.is_json());  // Content-Type viewed through Headers::view_of_beast
}

TEST(DriverHelpersTest, MakeBeastResponseTranslatesStatusHeadersBodyZeroCopy) {
    Response r;  // default new_delete (cold-path ok for this pure test)
    r.status     = HttpStatus::created;
    r.keep_alive = false;
    r.set_header("Content-Type", "application/json");
    r.add_header("Set-Cookie", "a=1");
    r.add_header("Set-Cookie", "b=2");
    r = std::move(r).with_body("{\"id\":1}");

    auto msg = detail::make_beast_response(r);
    EXPECT_EQ(msg.result_int(), 201u);
    EXPECT_EQ(msg.version(), 11u);
    EXPECT_EQ(std::string(msg[http::field::content_type]), "application/json");
    auto cookies = msg.equal_range("Set-Cookie");
    EXPECT_EQ(std::distance(cookies.first, cookies.second), 2);
    // buffer_body points AT the Response's body bytes — no copy.
    EXPECT_EQ(static_cast<const char*>(msg.body().data), r.body.buffered_view()->data());
    EXPECT_EQ(msg.body().size, 8u);
    EXPECT_FALSE(msg.keep_alive());
}

TEST(DriverHelpersTest, MakeBeastResponseEmptyBody) {
    Response r;
    r.status = HttpStatus::no_content;
    auto msg = detail::make_beast_response(r);
    EXPECT_EQ(msg.result_int(), 204u);
    EXPECT_EQ(msg.body().size, 0u);
}
