#include <array>
#include <atomic>
#include <cstddef>
#include <memory_resource>
#include <new>
#include <string>

#include <body.hpp>
#include <gtest/gtest.h>
#include <headers.hpp>
#include <request.hpp>
#include <request_context.hpp>
#include <response.hpp>

using namespace demiplane::http;

// ── Global operator new/delete instrumentation ──────────────────────────────
// Replacing these in one TU instruments the whole test binary (that is how
// global replacement works), but counting is armed only inside the measured
// regions via a thread_local flag, so gtest/runtime allocations elsewhere
// don't perturb the count. This intercepts what a pmr default-resource counter
// CANNOT: plain std::string/std::vector and coroutine-frame allocations —
// exactly the accidental classes this gate exists to catch.
namespace {
    std::atomic<std::size_t> g_armed_allocs{0};
    thread_local bool t_armed = false;

    struct ArmedRegion {  // counts allocs in this thread's region
        std::size_t start = g_armed_allocs.load(std::memory_order_relaxed);
        ArmedRegion() {
            t_armed = true;
        }
        [[nodiscard]] std::size_t finish() const {
            t_armed = false;
            return g_armed_allocs.load(std::memory_order_relaxed) - start;
        }
    };
}  // namespace

void* operator new(std::size_t size) {
    if (t_armed)
        g_armed_allocs.fetch_add(1, std::memory_order_relaxed);
    if (void* p = std::malloc(size))
        return p;
    throw std::bad_alloc{};
}
void* operator new(std::size_t size, std::align_val_t align) {
    if (t_armed)
        g_armed_allocs.fetch_add(1, std::memory_order_relaxed);
    const auto a              = static_cast<std::size_t>(align);
    const std::size_t rounded = (size + a - 1) / a * a;  // aligned_alloc precondition
    if (void* p = std::aligned_alloc(a, rounded))
        return p;
    throw std::bad_alloc{};
}
void operator delete(void* p) noexcept {
    std::free(p);
}
void operator delete(void* p, std::size_t) noexcept {
    std::free(p);
}
void operator delete(void* p, std::align_val_t) noexcept {
    std::free(p);
}
void operator delete(void* p, std::size_t, std::align_val_t) noexcept {
    std::free(p);
}
// (The default operator new[]/delete[] and nothrow forms forward to the above.)

TEST(AllocationGateTest, CtxOkPerformsNoHeapAllocations) {
    // Stack-backed arena — mirrors the real RequestArena (§6.1) and never
    // reaches operator new. A size-only monotonic_buffer_resource{8192} would
    // pull its first block from new_delete_resource INSIDE the armed region
    // and spuriously fail the gate. Do NOT "simplify" to the size-only ctor.
    std::array<std::byte, 8192> buf{};
    std::pmr::monotonic_buffer_resource arena{buf.data(), buf.size()};
    std::pmr::polymorphic_allocator<> arena_alloc{&arena};

    std::string target = "/health";
    Request req{Headers::owned(arena_alloc)};
    req.target = target;
    RequestContext ctx{std::move(req), arena_alloc};

    ArmedRegion region;
    Response r               = ctx.ok();  // empty body — pure framework path
    const std::size_t allocs = region.finish();

    EXPECT_EQ(allocs, 0u) << "ctx.ok() touched the heap";
    EXPECT_EQ(*r.headers.get("Content-Type"), "text/plain");
}

TEST(AllocationGateTest, CtxJsonAllocatesNothingBeyondTheUserBody) {
    std::array<std::byte, 8192> buf{};
    std::pmr::monotonic_buffer_resource arena{buf.data(), buf.size()};  // stack-backed (see CtxOk note)
    std::pmr::polymorphic_allocator<> arena_alloc{&arena};

    std::string target = "/users/42";
    Request req{Headers::owned(arena_alloc)};
    req.target = target;
    RequestContext ctx{std::move(req), arena_alloc};

    // The user's body — constructed BEFORE the measured region, then moved in.
    // Sized past SSO: an accidental copy inside the framework WOULD allocate
    // and fail the gate.
    std::string payload = R"({"id":42,"name":"long enough to defeat any SSO buffer"})";

    ArmedRegion region;
    Response r               = ctx.json(std::move(payload));  // moved; the framework must not copy
    const std::size_t allocs = region.finish();

    EXPECT_EQ(allocs, 0u) << "ctx.json() framework path touched the heap";
    EXPECT_EQ(*r.body.buffered_view(), R"({"id":42,"name":"long enough to defeat any SSO buffer"})");
}
