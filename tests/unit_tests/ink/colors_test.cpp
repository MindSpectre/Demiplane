#include <demiplane/ink>

#include <gtest/gtest.h>

namespace ink = demiplane::ink;

TEST(InkColors, ColorizeWrapsWithCodeAndReset) {
    const auto s = ink::colors::colorize(ink::colors::red, "hi");
    EXPECT_EQ(s, "\033[0;31mhi\033[0m");
}

TEST(InkColors, MakeRedRoundTrip) {
    EXPECT_EQ(ink::colors::make_red("err"), "\033[0;31merr\033[0m");
}

TEST(InkColors, MakeBoldRedRoundTrip) {
    EXPECT_EQ(ink::colors::make_bold_red("FATAL"), "\033[1;31mFATAL\033[0m");
}
