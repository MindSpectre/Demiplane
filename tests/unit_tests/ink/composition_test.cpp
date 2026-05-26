#include <demiplane/ink>
#include <string>

#include <gtest/gtest.h>

namespace ink = demiplane::ink;

TEST(InkComposition, BoxAroundTableRendersCleanly) {
    const auto table_str = ink::table().headers("a", "b").add_row(1, 2).render();
    // Default border is unicode; the inner table's widest line is still 9 visible columns.
    const auto got       = ink::box(table_str).title("Users").render();

    // inner = 9, padding=1, span = 9+2 = 11.
    // title_span = 5+2 = 7; min_inner = 7+2 = 9. span = max(11, 9) = 11.
    // Top: tl + ─*2 + " Users " + ─*2 + tr  (┌── Users ──┐). Byte-exact below.
    constexpr std::string_view expected =
        "\xE2\x94\x8C\xE2\x94\x80\xE2\x94\x80 Users \xE2\x94\x80\xE2\x94\x80\xE2\x94\x90\n"
        "\xE2\x94\x82 "
        "\xE2\x94\x8C\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\xAC\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x90 "
        "\xE2\x94\x82\n"
        "\xE2\x94\x82 \xE2\x94\x82 a \xE2\x94\x82 b \xE2\x94\x82 \xE2\x94\x82\n"
        "\xE2\x94\x82 "
        "\xE2\x94\x9C\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\xBC\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\xA4 "
        "\xE2\x94\x82\n"
        "\xE2\x94\x82 \xE2\x94\x82 1 \xE2\x94\x82 2 \xE2\x94\x82 \xE2\x94\x82\n"
        "\xE2\x94\x82 "
        "\xE2\x94\x94\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\xB4\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x98 "
        "\xE2\x94\x82\n"
        "\xE2\x94\x94\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80"
        "\xE2\x94\x80\xE2\x94\x80\xE2\x94\x80\xE2\x94\x98";
    EXPECT_EQ(got, expected);
}

TEST(InkComposition, SectionContainsPreStyledValue) {
    const auto got = ink::section("Info").row("Status", ink::colors::make_green("OK")).row("Count", 1).render();

    const std::string ok_green = ink::colors::make_green("OK");
    // max_label = 6 ("Status"); value_col = 2+6+1+2 = 11.
    // "  Status:  <ok_green>"
    // "  Count:   1"
    const std::string expected = std::string{"Info\n"} + "  Status:  " + ok_green + "\n" + "  Count:   1";
    EXPECT_EQ(got, expected);
}
