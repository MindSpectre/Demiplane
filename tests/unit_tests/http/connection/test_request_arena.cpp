#include <cstddef>
#include <memory_resource>

#include <gtest/gtest.h>

#include <request_arena.hpp>

using demiplane::http::RequestArena;

TEST(RequestArenaTest, AllocatesFromTheInitialBlock) {
    RequestArena arena{1024};
    auto alloc = arena.allocator();
    void* p = alloc.allocate_bytes(64, alignof(std::max_align_t));
    ASSERT_NE(p, nullptr);
}

TEST(RequestArenaTest, ResetRewindsAndReusesTheSameBlock) {
    RequestArena arena{1024};
    void* first = arena.allocator().allocate_bytes(128, 1);
    arena.reset();
    void* second = arena.allocator().allocate_bytes(128, 1);
    // After reset the monotonic resource hands back the start of the initial
    // block again — same address, no new heap block.
    EXPECT_EQ(first, second);
}

TEST(RequestArenaTest, AllocatorPointsAtThisArenasResource) {
    RequestArena a{512};
    RequestArena b{512};
    EXPECT_NE(a.allocator().resource(), b.allocator().resource());
    EXPECT_EQ(a.allocator().resource(), a.allocator().resource());
}
