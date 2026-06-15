#include <cstddef>
#include <span>
#include <string>

#include <body.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/use_future.hpp>
#include <gears_outcome.hpp>
#include <gtest/gtest.h>

using namespace demiplane::http;

namespace {
    template <typename T>
    T run_awaitable(boost::asio::awaitable<T> aw) {
        boost::asio::io_context ioc;
        auto fut = boost::asio::co_spawn(ioc, std::move(aw), boost::asio::use_future);
        ioc.run();
        return fut.get();
    }
}  // namespace

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
    EXPECT_EQ(*src.buffered_view(), "");  // NOLINT(bugprone-use-after-move) moved-from is a valid EmptyBody
    EXPECT_EQ(src.size_hint().value_or(99), 0u);
}

TEST(BodyTest, MoveAssignDestroysOldPayload) {
    Body a = Body::owned("aaa");
    Body b = Body::owned("bbb");
    a      = std::move(b);
    EXPECT_EQ(*a.buffered_view(), "bbb");
    EXPECT_EQ(*b.buffered_view(), "");  // NOLINT(bugprone-use-after-move) intentionally inspects moved-from state
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
    std::string body           = "--X\r\nContent-Disposition: form-data; name=\"field\"\r\n\r\nvalue\r\n--X--\r\n";
    Body b                     = Body::owned(body);
    auto o                     = run_awaitable(b.read_multipart(4096, boundary));
    ASSERT_TRUE(o.is_success());
    ASSERT_EQ(o.value().size(), 1u);
    EXPECT_EQ(o.value()[0].name, "field");
    EXPECT_EQ(o.value()[0].value, "value");
}
TEST(BodyTest, ReadMultipartFilenameDoesNotLeakIntoName) {
    const std::string boundary = "X";
    // filename appears BEFORE name; a naive find("name") matches inside "filename".
    std::string body           = "--X\r\nContent-Disposition: form-data; filename=\"doc.txt\"; name=\"file\"\r\n"
                                 "Content-Type: text/plain\r\n\r\nDATA\r\n--X--\r\n";
    Body b                     = Body::owned(body);
    auto o                     = run_awaitable(b.read_multipart(4096, boundary));
    ASSERT_TRUE(o.is_success());
    ASSERT_EQ(o.value().size(), 1u);
    EXPECT_EQ(o.value()[0].name, "file");
    EXPECT_EQ(o.value()[0].filename, "doc.txt");
    EXPECT_EQ(o.value()[0].content_type, "text/plain");
    EXPECT_EQ(o.value()[0].value, "DATA");
}

TEST(BodyTest, BeastViewBorrowsExternalBytes) {
    const std::string owner = "borrowed payload";
    Body b =
        Body::beast_view(std::span<const std::byte>{reinterpret_cast<const std::byte*>(owner.data()), owner.size()});

    ASSERT_TRUE(b.buffered_view().has_value());
    EXPECT_EQ(*b.buffered_view(), "borrowed payload");
    EXPECT_EQ(b.buffered_view()->data(), owner.data());  // zero-copy: same address
    EXPECT_EQ(b.size_hint().value_or(0), owner.size());
}

TEST(BodyTest, BeastViewYieldsOneChunkThenEnd) {
    const std::string owner = "abc";
    Body b =
        Body::beast_view(std::span<const std::byte>{reinterpret_cast<const std::byte*>(owner.data()), owner.size()});
    auto first = run_awaitable(b.read_chunk());
    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(first->size(), 3u);
    EXPECT_FALSE(run_awaitable(b.read_chunk()).has_value());
}

TEST(BodyTest, BeastViewEmptyYieldsNoChunks) {
    Body b = Body::beast_view(std::span<const std::byte>{});
    EXPECT_FALSE(run_awaitable(b.read_chunk()).has_value());
    ASSERT_TRUE(b.buffered_view().has_value());
    EXPECT_EQ(*b.buffered_view(), "");
}
