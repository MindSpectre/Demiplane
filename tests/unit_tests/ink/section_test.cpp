#include <gtest/gtest.h>

#include <string>

#include <demiplane/ink>

namespace ink = demiplane::ink;

TEST(InkSection, TitleOnly) {
    EXPECT_EQ(ink::section("Summary").render(), "Summary");
}

TEST(InkSection, LabelValueBlockAutoAlignsLabels) {
    const auto got = ink::section("Summary")
                         .row("Users", 42)
                         .row("Latency p99", "12ms")
                         .render();

    // max_label = 11 ("Latency p99"), default indent 2, sep "  " (2 spaces after colon).
    // Column positions: "  Users:" + (11-5) + 2 spaces = 8 spaces → "  Users:        42"
    constexpr std::string_view expected =
        "Summary\n"
        "  Users:        42\n"
        "  Latency p99:  12ms";

    EXPECT_EQ(got, expected);
}

TEST(InkSection, MultiLineValueIndentsContinuation) {
    const auto got = ink::section("Block")
                         .row("Config", "{\n  host: 1\n}")
                         .row("Users", 1)
                         .render();

    // max_label = 6 ("Config"), indent = 2, sep_width = 2.
    // value_col = 2 + 6 + 1 + 2 = 11.
    constexpr std::string_view expected =
        "Block\n"
        "  Config:  {\n"
        "             host: 1\n"
        "           }\n"
        "  Users:   1";

    EXPECT_EQ(got, expected);
}

TEST(InkSection, NoRowsJustTitle) {
    EXPECT_EQ(ink::section("X").render(), "X");
}

TEST(InkSection, TerminateAddsNewline) {
    const auto got = ink::section("X").row("a", 1).terminate().render();
    ASSERT_FALSE(got.empty());
    EXPECT_EQ(got.back(), '\n');
    EXPECT_NE(got[got.size() - 2], '\n');
}

TEST(InkSection, ValueAlignRightPadsToWidestValue) {
    const auto got = ink::section("Summary")
                         .row("ops", 42)
                         .row("latency", "1234567")
                         .value_align(ink::Align::Right)
                         .render();

    // Labels: max = 7 ("latency"). Values: max visible width = 7 ("1234567").
    // Row "ops":     "  ops:" + 6 spaces (max_label - 3 + separator(2)) + pad("42", 7, Right)=5sp+"42"
    //            →   11 spaces total between the colon and "42".
    // Row "latency": "  latency:" + 2 spaces (separator) + "1234567" (already width 7).
    constexpr std::string_view expected =
        "Summary\n"
        "  ops:           42\n"
        "  latency:  1234567";

    EXPECT_EQ(got, expected);
}

TEST(InkSection, ValueAlignLeftKeepsLegacyOutputWithoutTrailingSpaces) {
    // Explicitly setting Left must match the default (no trailing whitespace on the last row).
    const auto got = ink::section("S")
                         .row("a", 1)
                         .row("bb", 22)
                         .value_align(ink::Align::Left)
                         .render();
    const auto got_default = ink::section("S").row("a", 1).row("bb", 22).render();
    EXPECT_EQ(got, got_default);
    EXPECT_NE(got.back(), ' ');
}
