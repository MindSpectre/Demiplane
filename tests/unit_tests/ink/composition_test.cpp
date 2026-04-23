#include <demiplane/ink>
#include <string>

#include <gtest/gtest.h>

namespace ink = demiplane::ink;

TEST(InkComposition, BoxAroundTableRendersCleanly) {
    const auto table_str = ink::table().headers("a", "b").add_row(1, 2).render();
    // Table's widest line has length 9: "+---+---+".
    const auto got       = ink::box(table_str).title("Users").render();

    // inner = 9, padding=1, span = 9+2 = 11.
    // title_span = 5+2 = 7; min_inner = 7+2 = 9. span = max(11, 9) = 11.
    // Top: "+" + "-"*((11-7)/2=2) + " Users " + "-"*((11-7)-2=2) + "+"
    //    = "+-- Users --+"
    constexpr std::string_view expected = "+-- Users --+\n"
                                          "| +---+---+ |\n"
                                          "| | a | b | |\n"
                                          "| +---+---+ |\n"
                                          "| | 1 | 2 | |\n"
                                          "| +---+---+ |\n"
                                          "+-----------+";
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
