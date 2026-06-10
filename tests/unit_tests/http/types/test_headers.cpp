#include <memory_resource>
#include <string>
#include <string_view>
#include <vector>

#include <boost/beast/http.hpp>
#include <gtest/gtest.h>

#include <headers.hpp>

using namespace demiplane::http;

class HeadersTest : public ::testing::Test {
protected:
    std::pmr::monotonic_buffer_resource resource_{4096};
    std::pmr::polymorphic_allocator<> alloc_{&resource_};
};

TEST_F(HeadersTest, OwnedAddAndGetCaseInsensitive) {
    Headers h = Headers::owned(alloc_);
    h.add("Content-Type", "application/json");
    auto ct = h.get("content-type");
    ASSERT_TRUE(ct.has_value());
    EXPECT_EQ(*ct, "application/json");
    EXPECT_FALSE(h.get("X-Missing").has_value());
}

TEST_F(HeadersTest, OwnedMultiValueGetAll) {
    Headers h = Headers::owned(alloc_);
    h.add("Set-Cookie", "a=1");
    h.add("Set-Cookie", "b=2");
    auto all = h.get_all("set-cookie", alloc_);
    ASSERT_EQ(all.size(), 2u);
    EXPECT_EQ(all[0], "a=1");
    EXPECT_EQ(all[1], "b=2");
    EXPECT_EQ(*h.get("set-cookie"), "a=1");   // first-occurrence
}

TEST_F(HeadersTest, SetReplacesAll) {
    Headers h = Headers::owned(alloc_);
    h.add("X-Tag", "first");
    h.add("X-Tag", "second");
    h.set("X-Tag", "only");
    auto all = h.get_all("x-tag", alloc_);
    ASSERT_EQ(all.size(), 1u);
    EXPECT_EQ(all[0], "only");
}

TEST_F(HeadersTest, Remove) {
    Headers h = Headers::owned(alloc_);
    h.add("X-A", "1");
    h.add("X-B", "2");
    h.remove("x-a");
    EXPECT_FALSE(h.get("X-A").has_value());
    EXPECT_TRUE(h.get("X-B").has_value());
}

TEST_F(HeadersTest, IterationInsertionOrder) {
    Headers h = Headers::owned(alloc_);
    h.add("Host", "example.com");
    h.add("User-Agent", "test");
    std::vector<std::string> names;
    for (auto const& [n, v] : h) names.emplace_back(n);
    ASSERT_EQ(names.size(), 2u);
    EXPECT_EQ(names[0], "Host");
    EXPECT_EQ(names[1], "User-Agent");
}

TEST_F(HeadersTest, BeastBackingViewsParsedFields) {
    boost::beast::http::fields fields;
    fields.insert("Content-Type", "text/plain");
    fields.insert("X-Custom", "value");
    Headers h = Headers::view_of_beast(fields);
    ASSERT_TRUE(h.get("Content-Type").has_value());
    EXPECT_EQ(*h.get("Content-Type"), "text/plain");
    EXPECT_TRUE(h.contains("x-custom"));
    // O(1)-per-step iteration must visit all entries:
    std::size_t n = 0;
    for (auto const& kv : h) { (void)kv; ++n; }
    EXPECT_EQ(n, 2u);
}

TEST_F(HeadersTest, MutationPromotesBeastToOwnedViaBoundAllocator) {
    boost::beast::http::fields fields;
    fields.insert("Content-Type", "text/plain");
    Headers h = Headers::view_of_beast(fields);
    h.promote_to_owned(alloc_);   // explicit allocator — never the global heap
    h.add("X-Custom", "value");
    EXPECT_TRUE(h.contains("Content-Type"));
    EXPECT_TRUE(h.contains("X-Custom"));
}

TEST_F(HeadersTest, GetOrFallback) {
    Headers h = Headers::owned(alloc_);
    h.add("X-Tag", "value");
    EXPECT_EQ(h.get_or("X-Tag", "fallback"), "value");
    EXPECT_EQ(h.get_or("X-Missing", "fallback"), "fallback");
}

TEST_F(HeadersTest, MoveAssignAdoptsSourceBackingNoCopy) {
    std::pmr::monotonic_buffer_resource other_res{4096};
    Headers a = Headers::owned(alloc_);
    a.add("X-Old", "1");
    Headers b = Headers::owned(&other_res);
    b.add("X-New", "2");
    const char* stolen = b.get("X-New")->data();   // points into other_res
    a = std::move(b);
    EXPECT_FALSE(a.get("X-Old").has_value());
    ASSERT_TRUE(a.get("X-New").has_value());
    // Adopted (buffer stolen), not element-copied into a's old allocator:
    // the value still lives at the same address.
    EXPECT_EQ(a.get("X-New")->data(), stolen);
}
