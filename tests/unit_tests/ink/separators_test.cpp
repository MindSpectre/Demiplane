#include <demiplane/ink>

#include <gtest/gtest.h>

namespace ink = demiplane::ink;

TEST(InkSeparators, NewlineDefault) {
    EXPECT_EQ(ink::newline(), "\n");
    EXPECT_EQ(ink::newline(3), "\n\n\n");
}

TEST(InkSeparators, HrBuildsRule) {
    EXPECT_EQ(ink::hr(5), "-----");
    EXPECT_EQ(ink::hr(4, '='), "====");
}

TEST(InkSeparators, IndentEveryLine) {
    EXPECT_EQ(ink::indent("a\nb", 2), "  a\n  b");
}

TEST(InkSeparators, IndentPreservesTrailingNewline) {
    // Trailing '\n' does NOT pull an indent onto an empty final line.
    EXPECT_EQ(ink::indent("a\n", 2), "  a\n");
}

TEST(InkSeparators, PadLeft) {
    EXPECT_EQ(ink::pad("hi", 5), "hi   ");
}

TEST(InkSeparators, PadRight) {
    EXPECT_EQ(ink::pad("hi", 5, ink::Align::Right), "   hi");
}

TEST(InkSeparators, PadCenter) {
    EXPECT_EQ(ink::pad("hi", 6, ink::Align::Center), "  hi  ");
    EXPECT_EQ(ink::pad("hi", 5, ink::Align::Center), " hi  ");  // odd padding: extra space goes right
}

TEST(InkSeparators, PadNoopWhenAlreadyWide) {
    EXPECT_EQ(ink::pad("hello", 3), "hello");
}

TEST(InkSeparators, VisibleWidthSkipsAnsi) {
    const std::string s = ink::colors::make_red("abc");
    EXPECT_EQ(ink::detail::visible_width(s), 3u);
}

TEST(InkSeparators, VisibleWidthPlainAscii) {
    EXPECT_EQ(ink::detail::visible_width("hello"), 5u);
    EXPECT_EQ(ink::detail::visible_width(""), 0u);
}

TEST(InkSeparators, PadAnsiAwareWidth) {
    const std::string red_hi = ink::colors::make_red("hi");
    const std::string padded = ink::pad(red_hi, 5);
    // 3 spaces appended after red "hi" (visible width 2, target 5).
    EXPECT_EQ(padded, red_hi + "   ");
}

TEST(InkSeparators, LinesSplit) {
    const auto vec = ink::detail::lines("a\nb\nc");
    ASSERT_EQ(vec.size(), 3u);
    EXPECT_EQ(vec[0], "a");
    EXPECT_EQ(vec[1], "b");
    EXPECT_EQ(vec[2], "c");
}

TEST(InkSeparators, LinesEmptyInputYieldsOneEmpty) {
    const auto vec = ink::detail::lines("");
    ASSERT_EQ(vec.size(), 1u);
    EXPECT_EQ(vec[0], "");
}

TEST(InkSeparators, LinesTrailingNewlineYieldsEmptyTail) {
    const auto vec = ink::detail::lines("a\n");
    ASSERT_EQ(vec.size(), 2u);
    EXPECT_EQ(vec[0], "a");
    EXPECT_EQ(vec[1], "");
}
