#include <string>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/use_future.hpp>
#include <gtest/gtest.h>

#include <body/body.hpp>
#include <gears_outcome.hpp>
#include <url_decode/url_decode.hpp>

using namespace demiplane::http;

namespace {
    template <typename T>
    T run_awaitable(boost::asio::awaitable<T> aw) {
        boost::asio::io_context ioc;
        auto fut = boost::asio::co_spawn(ioc, std::move(aw), boost::asio::use_future);
        ioc.run();
        return fut.get();
    }
}

TEST(BodyTest, DefaultIsEmpty) {
    Body b;
    EXPECT_EQ(b.size_hint().value_or(99), 0u);
    ASSERT_TRUE(b.buffered_view().has_value());
    EXPECT_EQ(*b.buffered_view(), "");
    EXPECT_FALSE(run_awaitable(b.read_chunk()).has_value());
}

TEST(BodyTest, OwnedBufferYieldsContentsThenEnd) {
    Body b = Body::owned("hello, world");
    ASSERT_TRUE(b.buffered_view().has_value());
    EXPECT_EQ(*b.buffered_view(), "hello, world");
    EXPECT_EQ(b.size_hint().value_or(0), 12u);

    auto first = run_awaitable(b.read_chunk());
    ASSERT_TRUE(first.has_value());
    std::string text(reinterpret_cast<const char*>(first->data()), first->size());
    EXPECT_EQ(text, "hello, world");
    EXPECT_FALSE(run_awaitable(b.read_chunk()).has_value());
}

TEST(BodyTest, OwnedEmptyYieldsNoChunks) {
    Body b = Body::owned("");
    EXPECT_FALSE(run_awaitable(b.read_chunk()).has_value());
    EXPECT_EQ(*b.buffered_view(), "");
}

TEST(BodyTest, MoveTransfersPayloadLeavesSourceEmpty) {
    Body src = Body::owned("payload");
    Body dst = std::move(src);
    EXPECT_EQ(*dst.buffered_view(), "payload");
    EXPECT_EQ(*src.buffered_view(), "");   // moved-from is a valid EmptyBody
    EXPECT_EQ(src.size_hint().value_or(99), 0u);
}

TEST(BodyTest, MoveAssignDestroysOldPayload) {
    Body a = Body::owned("aaa");
    Body b = Body::owned("bbb");
    a = std::move(b);
    EXPECT_EQ(*a.buffered_view(), "bbb");
    EXPECT_EQ(*b.buffered_view(), "");
}

TEST(BodyTest, ReadToStringSucceeds) {
    Body b = Body::owned("hello");
    auto o = run_awaitable(b.read_to_string(100));
    ASSERT_TRUE(o.is_success());
    EXPECT_EQ(o.value(), "hello");
}
TEST(BodyTest, ReadToStringLimitExceeded) {
    Body b = Body::owned("hello");
    auto o = run_awaitable(b.read_to_string(3));
    ASSERT_TRUE(o.is_error());
    EXPECT_TRUE(o.holds_error<BodyLimitExceeded>());
}
TEST(BodyTest, ReadJsonSucceeds) {
    Body b = Body::owned(R"({"a":1,"b":"two"})");
    auto o = run_awaitable(b.read_json(1024));
    ASSERT_TRUE(o.is_success());
    EXPECT_EQ(o.value()["a"].asInt(), 1);
    EXPECT_EQ(o.value()["b"].asString(), "two");
}
TEST(BodyTest, ReadJsonMalformed) {
    Body b = Body::owned("not json");
    auto o = run_awaitable(b.read_json(1024));
    ASSERT_TRUE(o.is_error());
    EXPECT_TRUE(o.holds_error<JsonParseError>());
}
TEST(BodyTest, ReadFormUrlDecodes) {
    Body b = Body::owned("name=John%20Doe&city=New+York&empty=");
    auto o = run_awaitable(b.read_form(1024));
    ASSERT_TRUE(o.is_success());
    EXPECT_EQ(o.value().at("name"), "John Doe");
    EXPECT_EQ(o.value().at("city"), "New York");
    EXPECT_EQ(o.value().at("empty"), "");
}
TEST(BodyTest, ReadFormEmptyKeyIsError) {
    Body b = Body::owned("=value");
    auto o = run_awaitable(b.read_form(1024));
    ASSERT_TRUE(o.is_error());
    EXPECT_TRUE(o.holds_error<FormParseError>());
}
TEST(BodyTest, ReadMultipartNoBoundaryIsError) {
    Body b = Body::owned("x");
    auto o = run_awaitable(b.read_multipart(1024, ""));
    ASSERT_TRUE(o.is_error());
    EXPECT_TRUE(o.holds_error<MultipartParseError>());
}
TEST(BodyTest, ReadMultipartWellFormed) {
    const std::string boundary = "X";
    std::string body =
        "--X\r\nContent-Disposition: form-data; name=\"field\"\r\n\r\nvalue\r\n--X--\r\n";
    Body b = Body::owned(body);
    auto o = run_awaitable(b.read_multipart(4096, boundary));
    ASSERT_TRUE(o.is_success());
    ASSERT_EQ(o.value().size(), 1u);
    EXPECT_EQ(o.value()[0].name, "field");
    EXPECT_EQ(o.value()[0].value, "value");
}
