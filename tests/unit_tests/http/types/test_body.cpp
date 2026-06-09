#include <string>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/use_future.hpp>
#include <gtest/gtest.h>

#include <body/body.hpp>

using namespace demiplane::http;

namespace {
    template <typename T>
    T run_awaitable(boost::asio::awaitable<T> aw) {
        boost::asio::io_context ioc;
        auto fut = boost::asio::co_spawn(ioc, std::move(aw), boost::asio::use_future);
        ioc.run();
        return fut.get();
    }
}

TEST(BodyTest, DefaultIsEmpty) {
    Body b;
    EXPECT_EQ(b.size_hint().value_or(99), 0u);
    ASSERT_TRUE(b.buffered_view().has_value());
    EXPECT_EQ(*b.buffered_view(), "");
    EXPECT_FALSE(run_awaitable(b.read_chunk()).has_value());
}

TEST(BodyTest, OwnedBufferYieldsContentsThenEnd) {
    Body b = Body::owned("hello, world");
    ASSERT_TRUE(b.buffered_view().has_value());
    EXPECT_EQ(*b.buffered_view(), "hello, world");
    EXPECT_EQ(b.size_hint().value_or(0), 12u);

    auto first = run_awaitable(b.read_chunk());
    ASSERT_TRUE(first.has_value());
    std::string text(reinterpret_cast<const char*>(first->data()), first->size());
    EXPECT_EQ(text, "hello, world");
    EXPECT_FALSE(run_awaitable(b.read_chunk()).has_value());
}

TEST(BodyTest, OwnedEmptyYieldsNoChunks) {
    Body b = Body::owned("");
    EXPECT_FALSE(run_awaitable(b.read_chunk()).has_value());
    EXPECT_EQ(*b.buffered_view(), "");
}

TEST(BodyTest, MoveTransfersPayloadLeavesSourceEmpty) {
    Body src = Body::owned("payload");
    Body dst = std::move(src);
    EXPECT_EQ(*dst.buffered_view(), "payload");
    EXPECT_EQ(*src.buffered_view(), "");   // moved-from is a valid EmptyBody
    EXPECT_EQ(src.size_hint().value_or(99), 0u);
}

TEST(BodyTest, MoveAssignDestroysOldPayload) {
    Body a = Body::owned("aaa");
    Body b = Body::owned("bbb");
    a = std::move(b);
    EXPECT_EQ(*a.buffered_view(), "bbb");
    EXPECT_EQ(*b.buffered_view(), "");
}
