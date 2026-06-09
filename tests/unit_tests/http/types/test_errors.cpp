#include <gtest/gtest.h>

#include <body.hpp>
#include <errors.hpp>
#include <response.hpp>

using namespace demiplane::http;

namespace { std::string body_of(const Response& r) {
    return std::string{r.body.buffered_view().value_or("")};
}}

TEST(ToHttpResponseTest, BadRequest400)   { auto r = to_http_response(BadRequestError{"x"}); EXPECT_EQ(r.status, HttpStatus::bad_request); EXPECT_EQ(body_of(r), "x"); }
TEST(ToHttpResponseTest, Unauthorized401) { EXPECT_EQ(to_http_response(UnauthorizedError{"t"}).status, HttpStatus::unauthorized); }
TEST(ToHttpResponseTest, Forbidden403)    { EXPECT_EQ(to_http_response(ForbiddenError{"n"}).status, HttpStatus::forbidden); }
TEST(ToHttpResponseTest, NotFound404)     { auto r = to_http_response(NotFoundError{"user","42"}); EXPECT_EQ(r.status, HttpStatus::not_found); EXPECT_EQ(body_of(r), "user 42 not found"); }
TEST(ToHttpResponseTest, Conflict409)     { EXPECT_EQ(to_http_response(ConflictError{"e"}).status, HttpStatus::conflict); }
TEST(ToHttpResponseTest, UnprocessableIncludesFields) {
    UnprocessableEntityError e{"bad", {{"name","required"},{"age","positive"}}};
    auto r = to_http_response(e);
    EXPECT_EQ(r.status, HttpStatus::unprocessable_entity);
    auto b = body_of(r);
    EXPECT_NE(b.find("name: required"), std::string::npos);
    EXPECT_NE(b.find("age: positive"), std::string::npos);
}
TEST(ToHttpResponseTest, PayloadTooLarge413) { auto r = to_http_response(PayloadTooLargeError{1024}); EXPECT_EQ(r.status, HttpStatus::payload_too_large); EXPECT_NE(body_of(r).find("1024"), std::string::npos); }
TEST(ToHttpResponseTest, MethodNotAllowedAllow) {
    MethodNotAllowedError e{{HttpMethod::get, HttpMethod::head}};
    auto r = to_http_response(e);
    EXPECT_EQ(r.status, HttpStatus::method_not_allowed);
    EXPECT_EQ(*r.headers.get("Allow"), "GET, HEAD");
}
TEST(ToHttpResponseTest, JsonParse400)     { EXPECT_EQ(to_http_response(JsonParseError{"oops"}).status, HttpStatus::bad_request); }
TEST(ToHttpResponseTest, BodyLimit413)     { EXPECT_EQ(to_http_response(BodyLimitExceeded{16}).status, HttpStatus::payload_too_large); }
