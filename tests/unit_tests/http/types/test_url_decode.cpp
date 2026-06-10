#include <gtest/gtest.h>
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
