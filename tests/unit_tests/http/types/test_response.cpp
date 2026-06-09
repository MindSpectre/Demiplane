#include <memory_resource>
#include <string>

#include <gtest/gtest.h>

#include <body/body.hpp>
#include <response/response.hpp>

using namespace demiplane::http;

TEST(ResponseTest, DefaultConstructible) {
    Response r;
    EXPECT_EQ(r.status, HttpStatus::ok);
    EXPECT_EQ(r.version, HttpVersion::http_1_1);
    EXPECT_TRUE(r.keep_alive);
    EXPECT_TRUE(r.headers.empty());
    ASSERT_TRUE(r.body.buffered_view().has_value());
    EXPECT_EQ(*r.body.buffered_view(), "");
}

TEST(ResponseTest, FluentSettersOnLValue) {
    Response r;
    r.with_status(HttpStatus::created)
     .set_header("Content-Type", "application/json")
     .with_body("{\"id\":42}");
    EXPECT_EQ(r.status, HttpStatus::created);
    ASSERT_TRUE(r.headers.get("content-type").has_value());
    EXPECT_EQ(*r.headers.get("content-type"), "application/json");
    EXPECT_EQ(*r.body.buffered_view(), "{\"id\":42}");
}

TEST(ResponseTest, FluentSettersOnRValue) {
    Response r = Response{}
        .with_status(HttpStatus::no_content)
        .set_header("X-Custom", "value");
    EXPECT_EQ(r.status, HttpStatus::no_content);
    EXPECT_EQ(*r.headers.get("X-Custom"), "value");
}

TEST(ResponseTest, SetHeaderReplacesAddHeaderAppends) {
    Response r;
    r.add_header("X-Tag", "a").add_header("X-Tag", "b");
    std::pmr::monotonic_buffer_resource res{1024};
    EXPECT_EQ(r.headers.get_all("x-tag", &res).size(), 2u);
    r.set_header("X-Tag", "only");
    EXPECT_EQ(r.headers.get_all("x-tag", &res).size(), 1u);
}

TEST(ResponseTest, ArenaConstructorBindsHeaderAllocator) {
    std::pmr::monotonic_buffer_resource res{4096};
    Response r{std::pmr::polymorphic_allocator<>{&res}};
    r.add_header("X-A", "1");   // must allocate in `res`, not the global heap
    EXPECT_TRUE(r.headers.contains("X-A"));
}
