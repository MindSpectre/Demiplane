#include <demiplane/ink>
#include <string>

#include <gtest/gtest.h>

namespace ink = demiplane::ink;

// The default border is unicode (border::unicode in glyphs.hpp); these are byte-exact.
TEST(InkBox, DefaultBorderBodyOnly) {
    const auto got = ink::box("hello").render();
    constexpr std::string_view expected =
        "\xE2\x94\x8C\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x90\n"
        "\xE2\x94\x82 hello \xE2\x94\x82\n"
        "\xE2\x94\x94\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x98";
    EXPECT_EQ(got, expected);
}

TEST(InkBox, MultiLineBody) {
    const auto got                      = ink::box("line one\nshort").render();
    // span = max visible widths = 8 ("line one"), plus 2*padding(1) = 10.
    constexpr std::string_view expected = "\xE2\x94\x8C\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2"
                                          "\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x90\n"
                                          "\xE2\x94\x82 line one \xE2\x94\x82\n"
                                          "\xE2\x94\x82 short    \xE2\x94\x82\n"
                                          "\xE2\x94\x94\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2"
                                          "\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x98";
    EXPECT_EQ(got, expected);
}

TEST(InkBox, TitleCenteredInTopBorder) {
    const auto got = ink::box("body").title("Hi").render();
    // inner (widest body line) = 4 ("body"). padding=1 default. span = 4 + 2*1 = 6.
    // min_inner_for_title = 2 + 2 + 2 = 6. span = max(6, 6) = 6.
    // title_span = 4. left = (6-4)/2 = 1, right = 6-4-1 = 1.
    // Top: tl + ─*1 + " Hi " + ─*1 + tr  (┌─ Hi ─┐)
    constexpr std::string_view expected =
        "\xE2\x94\x8C\xE2\x94\x80 Hi \xE2\x94\x80\xE2\x94\x90\n"
        "\xE2\x94\x82 body \xE2\x94\x82\n"
        "\xE2\x94\x94\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x98";
    EXPECT_EQ(got, expected);
}

TEST(InkBox, TitleWidensBoxWhenLarger) {
    const auto got = ink::box("hi").title("LongTitle").render();
    // body_inner = 2, padding = 1 → span candidate = 4.
    // title span = 9+2 = 11; min_inner = 11 + 2 = 13. span = max(4, 13) = 13.
    // Top: tl + ─*1 + " LongTitle " + ─*1 + tr  (┌─ LongTitle ─┐)
    // Body line width = span (13) = inner + 2*padding, so inner = 11. "hi" → "hi" + 9 spaces.
    constexpr std::string_view expected =
        "\xE2\x94\x8C\xE2\x94\x80 LongTitle \xE2\x94\x80\xE2\x94\x90\n"
        "\xE2\x94\x82 hi          \xE2\x94\x82\n"
        "\xE2\x94\x94\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80"
        "\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x98";
    EXPECT_EQ(got, expected);
}

TEST(InkBox, UnicodeBorderByteExact) {
    const auto got                      = ink::box("x").border(ink::border::unicode).render();
    constexpr std::string_view expected = "\xE2\x94\x8C\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x90\n"  // ┌───┐
                                          "\xE2\x94\x82 x \xE2\x94\x82\n"                                   // │ x │
                                          "\xE2\x94\x94\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x98";   // └───┘
    EXPECT_EQ(got, expected);
}

TEST(InkBox, TerminateAddsNewline) {
    const auto got = ink::box("x").terminate().render();
    ASSERT_FALSE(got.empty());
    EXPECT_EQ(got.back(), '\n');
}
