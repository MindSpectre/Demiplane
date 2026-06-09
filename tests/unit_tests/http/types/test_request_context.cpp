#include <deque>
#include <memory_resource>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <body/body.hpp>
#include <headers/headers.hpp>
#include <request/request.hpp>
#include <request_context/request_context.hpp>

using namespace demiplane::http;

class RequestContextTest : public ::testing::Test {
protected:
    std::pmr::monotonic_buffer_resource resource_{8192};
    std::pmr::polymorphic_allocator<> alloc_{&resource_};
    std::deque<std::string> target_storage_;   // stable backing for string_view targets

    Request make_request(HttpMethod m, std::string target,
                         std::vector<std::pair<std::string, std::string>> hdrs = {},
                         std::string body_text = "") {
        Request req{Headers::owned(alloc_)};
        req.method  = m;
        req.version = HttpVersion::http_1_1;
        target_storage_.push_back(std::move(target));
        req.target  = target_storage_.back();          // view into stable storage
        for (auto const& [k, v] : hdrs) req.headers.add(k, v);
        req.body = body_text.empty() ? Body::empty() : Body::owned(std::move(body_text));
        return req;
    }
};

TEST_F(RequestContextTest, MethodTargetVersion) {
    RequestContext ctx{make_request(HttpMethod::get, "/users"), alloc_};
    EXPECT_EQ(ctx.method(), HttpMethod::get);
    EXPECT_EQ(ctx.target(), "/users");
    EXPECT_EQ(ctx.version(), HttpVersion::http_1_1);
}
TEST_F(RequestContextTest, HeaderLookup) {
    RequestContext ctx{make_request(HttpMethod::get, "/", {{"Host", "example.com"}}), alloc_};
    ASSERT_TRUE(ctx.header("host").has_value());
    EXPECT_EQ(*ctx.header("host"), "example.com");
    EXPECT_FALSE(ctx.header("missing").has_value());
}
TEST_F(RequestContextTest, BodyAccess) {
    RequestContext ctx{make_request(HttpMethod::post, "/", {}, "hello"), alloc_};
    EXPECT_EQ(ctx.body().size_hint().value_or(0), 5u);
}
TEST_F(RequestContextTest, ContentTypePredicates) {
    auto json_ctx  = RequestContext{make_request(HttpMethod::post, "/", {{"Content-Type","application/json"}}), alloc_};
    auto form_ctx  = RequestContext{make_request(HttpMethod::post, "/", {{"Content-Type","application/x-www-form-urlencoded"}}), alloc_};
    auto multi_ctx = RequestContext{make_request(HttpMethod::post, "/", {{"Content-Type","multipart/form-data; boundary=xx"}}), alloc_};
    EXPECT_TRUE(json_ctx.is_json());
    EXPECT_TRUE(form_ctx.is_form());
    EXPECT_TRUE(multi_ctx.is_multipart());
}
TEST_F(RequestContextTest, PathSplit) {
    RequestContext ctx{make_request(HttpMethod::get, "/users/42?q=foo&p=bar"), alloc_};
    EXPECT_EQ(ctx.path(), "/users/42");
    EXPECT_EQ(ctx.query_string(), "q=foo&p=bar");
}
TEST_F(RequestContextTest, PathSplitNoQuery) {
    RequestContext ctx{make_request(HttpMethod::get, "/users/42"), alloc_};
    EXPECT_EQ(ctx.path(), "/users/42");
    EXPECT_EQ(ctx.query_string(), "");
}
TEST_F(RequestContextTest, CachedPathSurvivesMove) {
    RequestContext a{make_request(HttpMethod::get, "/u"), alloc_};  // short (SSO-length) target
    EXPECT_EQ(a.path(), "/u");                                      // populate the cache
    RequestContext b{std::move(a)};                                 // move after caching
    EXPECT_EQ(b.path(), "/u");                                      // view still valid (target is a view)
}
