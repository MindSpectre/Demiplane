#include <gtest/gtest.h>
#include <prefix_filter.hpp>

using demiplane::scroll::PrefixFilter;

TEST(PrefixFilterTest, AllowAllAdmitsEverythingIncludingEmpty) {
    PrefixFilter f;
    EXPECT_TRUE(f.accepts("foo"));
    EXPECT_TRUE(f.accepts("bar"));
    EXPECT_TRUE(f.accepts(""));
}

TEST(PrefixFilterTest, AllowListAdmitsListedAndEmpty) {
    auto f = PrefixFilter::allow({"Foo", "Bar"});
    EXPECT_TRUE(f.accepts("Foo"));
    EXPECT_TRUE(f.accepts("Bar"));
    EXPECT_FALSE(f.accepts("Baz"));
    EXPECT_TRUE(f.accepts("")) << "empty prefix admitted by default";
}

TEST(PrefixFilterTest, AllowListWithBlockEmptyRejectsEmpty) {
    auto f = PrefixFilter::allow({"Foo"}).block_empty_prefix();
    EXPECT_TRUE(f.accepts("Foo"));
    EXPECT_FALSE(f.accepts("Bar"));
    EXPECT_FALSE(f.accepts(""));
}

TEST(PrefixFilterTest, DenyListAdmitsEverythingExceptListed) {
    auto f = PrefixFilter::deny({"Noisy"});
    EXPECT_FALSE(f.accepts("Noisy"));
    EXPECT_TRUE(f.accepts("Quiet"));
    EXPECT_TRUE(f.accepts(""));
}

TEST(PrefixFilterTest, DenyListWithBlockEmptyRejectsEmpty) {
    auto f = PrefixFilter::deny({"Noisy"}).block_empty_prefix();
    EXPECT_FALSE(f.accepts("Noisy"));
    EXPECT_TRUE(f.accepts("Quiet"));
    EXPECT_FALSE(f.accepts(""));
}

TEST(PrefixFilterTest, HeterogeneousLookupAcceptsStringView) {
    auto f = PrefixFilter::allow({"Foo"});
    std::string_view sv{"Foo"};
    EXPECT_TRUE(f.accepts(sv));
    EXPECT_FALSE(f.accepts(std::string_view{"Bar"}));
}
