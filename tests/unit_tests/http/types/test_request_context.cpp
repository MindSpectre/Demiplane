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
TEST_F(RequestContextTest, QueryUrlDecodedTypedConversions) {
    RequestContext ctx{make_request(HttpMethod::get, "/?name=John%20Doe&n=42&city=New+York"), alloc_};
    EXPECT_EQ(ctx.query<std::string>("name").value_or(""), "John Doe");
    EXPECT_EQ(ctx.query<int>("n").value_or(0), 42);
    EXPECT_EQ(ctx.query<std::string>("city").value_or(""), "New York");
    EXPECT_FALSE(ctx.query<int>("missing").has_value());
}
TEST_F(RequestContextTest, QueryArbitraryArithmeticTypesLink) {
    RequestContext ctx{make_request(HttpMethod::get, "/?p=7"), alloc_};
    EXPECT_EQ(ctx.query<std::size_t>("p").value_or(0), 7u);   // these would NOT link in the old plan
    EXPECT_EQ(ctx.query<unsigned>("p").value_or(0), 7u);
    EXPECT_DOUBLE_EQ(ctx.query<double>("p").value_or(0.0), 7.0);
}
TEST_F(RequestContextTest, QueryOrFallback) {
    RequestContext ctx{make_request(HttpMethod::get, "/?n=10"), alloc_};
    EXPECT_EQ(ctx.query_or<int>("n", 99), 10);
    EXPECT_EQ(ctx.query_or<int>("missing", 99), 99);
}
TEST_F(RequestContextTest, PathParamSetAndConvert) {
    RequestContext ctx{make_request(HttpMethod::get, "/users/42"), alloc_};
    ctx.set_path_param("id", "42");
    EXPECT_EQ(ctx.path_param<int>("id").value_or(0), 42);
    EXPECT_EQ(ctx.path_param_or<std::string>("id", "x"), "42");
    EXPECT_FALSE(ctx.path_param<int>("missing").has_value());
}
TEST_F(RequestContextTest, PathParamConvertFailure) {
    RequestContext ctx{make_request(HttpMethod::get, "/users/abc"), alloc_};
    ctx.set_path_param("id", "abc");
    EXPECT_FALSE(ctx.path_param<int>("id").has_value());
    EXPECT_EQ(ctx.path_param<std::string>("id").value_or(""), "abc");
}
