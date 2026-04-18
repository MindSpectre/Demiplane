#include <gears_strings.hpp>
#include <gtest/gtest.h>

using demiplane::gears::InlineString;

TEST(InlineStringTest, AssignStoresExactString) {
    InlineString<31> s;
    s.assign(std::string_view{"hello"});
    EXPECT_EQ(s.view(), "hello");
    EXPECT_EQ(s.size(), 5u);
    EXPECT_STREQ(s.c_str(), "hello");
}

TEST(InlineStringTest, AssignTruncatesOverflowAtRuntime) {
    InlineString<4> s;
    s.assign(std::string_view{"too long for four"});
    EXPECT_EQ(s.view(), "too ");
    EXPECT_EQ(s.size(), 4u);
    EXPECT_STREQ(s.c_str(), "too ");
}

TEST(InlineStringTest, AssignOverwritesPrior) {
    InlineString<31> s;
    s.assign(std::string_view{"first"});
    s.assign(std::string_view{"second longer"});
    EXPECT_EQ(s.view(), "second longer");
    EXPECT_EQ(s.size(), 13u);
}

TEST(InlineStringTest, AssignEmptyClears) {
    InlineString<31> s;
    s.assign(std::string_view{"nonempty"});
    s.assign(std::string_view{});
    EXPECT_TRUE(s.empty());
    EXPECT_EQ(s.size(), 0u);
    EXPECT_STREQ(s.c_str(), "");
}

TEST(InlineStringTest, ClearResetsToEmpty) {
    InlineString<31> s;
    s.assign(std::string_view{"content"});
    s.clear();
    EXPECT_TRUE(s.empty());
    EXPECT_EQ(s.size(), 0u);
    EXPECT_STREQ(s.c_str(), "");
}

TEST(InlineStringTest, ClearIsIdempotent) {
    InlineString<31> s;
    s.clear();
    EXPECT_TRUE(s.empty());
    EXPECT_STREQ(s.c_str(), "");

    s.assign(std::string_view{"payload"});
    s.clear();
    s.clear();
    EXPECT_TRUE(s.empty());
    EXPECT_EQ(s.size(), 0u);
    EXPECT_STREQ(s.c_str(), "");
}

TEST(InlineStringTest, AssignAtCompileTimeFitting) {
    constexpr auto s = [] {
        InlineString<31> x;
        x.assign(std::string_view{"Compile"});
        return x;
    }();
    static_assert(s.view() == std::string_view{"Compile"});
    static_assert(s.size() == 7u);
    EXPECT_EQ(s.view(), "Compile");
}

// Compile-time overflow demo (intentionally NOT compiled — switching to 1 would fail to build):
// constexpr auto fails = [] {
//     InlineString<2> x;
//     x.assign(std::string_view{"three"});   // triggers throw at consteval → compile error
//     return x;
// }();

TEST(InlineStringTest, AssignExactlyCapacityFitsWithoutTruncation) {
    InlineString<5> s;
    s.assign(std::string_view{"hello"});
    EXPECT_EQ(s.view(), "hello");
    EXPECT_EQ(s.size(), 5u);
}
