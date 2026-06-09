#include <gtest/gtest.h>

#include <body/body.hpp>
#include <response_factory/response_factory.hpp>

using namespace demiplane::http;

namespace { std::string body_of(const Response& r) {
    return std::string{r.body.buffered_view().value_or("")};
}}

TEST(ResponseFactoryTest, OkDefault) {
    auto r = ResponseFactory::ok();
    EXPECT_EQ(r.status, HttpStatus::ok);
    EXPECT_EQ(*r.headers.get("Content-Type"), "text/plain");
    EXPECT_EQ(body_of(r), "");
}
TEST(ResponseFactoryTest, JsonContentType) {
    auto r = ResponseFactory::json("{\"k\":\"v\"}");
    EXPECT_EQ(*r.headers.get("Content-Type"), "application/json");
    EXPECT_EQ(body_of(r), "{\"k\":\"v\"}");
}
TEST(ResponseFactoryTest, NoContentHasNoBodyNorContentType) {
    auto r = ResponseFactory::no_content();
    EXPECT_EQ(r.status, HttpStatus::no_content);
    EXPECT_EQ(body_of(r), "");
    EXPECT_FALSE(r.headers.contains("Content-Type"));
}
TEST(ResponseFactoryTest, RedirectSetsLocation) {
    auto r = ResponseFactory::redirect("/login");
    EXPECT_EQ(r.status, HttpStatus::found);
    EXPECT_EQ(*r.headers.get("Location"), "/login");
}
TEST(ResponseFactoryTest, MethodNotAllowedSetsAllow) {
    HttpMethod allow[] = {HttpMethod::get, HttpMethod::post};
    auto r = ResponseFactory::method_not_allowed(allow);
    EXPECT_EQ(r.status, HttpStatus::method_not_allowed);
    EXPECT_EQ(*r.headers.get("Allow"), "GET, POST");
}
TEST(ResponseFactoryTest, NotFound) {
    auto r = ResponseFactory::not_found();
    EXPECT_EQ(r.status, HttpStatus::not_found);
    EXPECT_EQ(body_of(r), "Not Found");
}
