#include <gtest/gtest.h>
#include <memory_resource>
#include <url_decode.hpp>

using namespace demiplane::http;

TEST(UrlDecodeTest, PlainPassthrough) {
    EXPECT_EQ(url_decode("hello").value(), "hello");
}
TEST(UrlDecodeTest, PercentEscape) {
    EXPECT_EQ(url_decode("John%20Doe").value(), "John Doe");
}
TEST(UrlDecodeTest, PlusIsSpaceByDefault) {
    EXPECT_EQ(url_decode("New+York").value(), "New York");
}
TEST(UrlDecodeTest, PlusLiteralWhenOff) {
    EXPECT_EQ(url_decode("a+b", false).value(), "a+b");
}
TEST(UrlDecodeTest, TruncatedEscapeFails) {
    EXPECT_FALSE(url_decode("a%2").has_value());
}
TEST(UrlDecodeTest, BadHexFails) {
    EXPECT_FALSE(url_decode("a%2G").has_value());
}
TEST(UrlDecodeTest, LowercaseHex) {
    EXPECT_EQ(url_decode("%2f").value(), "/");
}

class UrlDecodeArenaTest : public ::testing::Test {
protected:
    std::pmr::monotonic_buffer_resource res_{1024};
    std::pmr::polymorphic_allocator<> alloc_{&res_};
};

TEST_F(UrlDecodeArenaTest, PassthroughIsZeroCopy) {
    const std::string_view in = "plain-segment";
    auto out = url_decode_arena(in, false, alloc_);
    ASSERT_TRUE(out.has_value());
    EXPECT_EQ(*out, "plain-segment");
    EXPECT_EQ(out->data(), in.data());  // same buffer — no copy was made
}

TEST_F(UrlDecodeArenaTest, DecodesEscapesIntoArena) {
    const std::string_view in = "John%20Doe";
    auto out = url_decode_arena(in, false, alloc_);
    ASSERT_TRUE(out.has_value());
    EXPECT_EQ(*out, "John Doe");
    EXPECT_NE(out->data(), in.data());  // rewrite path wrote to the arena, not the input
}

TEST_F(UrlDecodeArenaTest, EmptyInputIsZeroCopy) {
    const std::string_view in{};
    auto out = url_decode_arena(in, false, alloc_);
    ASSERT_TRUE(out.has_value());
    EXPECT_TRUE(out->empty());
}

TEST_F(UrlDecodeArenaTest, PlusIsLiteralInPathMode) {
    const std::string_view in = "a+b";
    auto out = url_decode_arena(in, false, alloc_);
    ASSERT_TRUE(out.has_value());
    EXPECT_EQ(*out, "a+b");
    EXPECT_EQ(out->data(), in.data());  // '+' alone forces no rewrite in path mode
}

TEST_F(UrlDecodeArenaTest, PlusIsSpaceInFormMode) {
    auto out = url_decode_arena("a+b", true, alloc_);
    ASSERT_TRUE(out.has_value());
    EXPECT_EQ(*out, "a b");
}

TEST_F(UrlDecodeArenaTest, MalformedEscapesFail) {
    EXPECT_FALSE(url_decode_arena("a%2", false, alloc_).has_value());
    EXPECT_FALSE(url_decode_arena("a%2G", false, alloc_).has_value());
}
