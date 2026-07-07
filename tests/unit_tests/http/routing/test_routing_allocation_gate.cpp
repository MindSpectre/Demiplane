#include <array>
#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <memory_resource>
#include <new>
#include <string>

#include <gtest/gtest.h>

#include <request_context.hpp>
#include <response.hpp>
#include <route_registry.hpp>
#include <router.hpp>

using namespace demiplane::http;

// ── Global operator new/delete instrumentation ──────────────────────────────
// Same mechanism as the PR 1 types gate (test_allocation_gate.cpp): replacing
// these in one TU instruments this whole test binary; counting is armed only
// inside measured regions via a thread_local flag. This sees what a pmr
// counter cannot: plain std::string/std::function/coroutine-frame allocations.
// Http.Routing and Http.Types are DIFFERENT test binaries — each can safely
// define its own global operator new/delete replacement (no ODR violation).
namespace {
    std::atomic<std::size_t> g_armed_allocs{0};
    thread_local bool t_armed = false;

    struct ArmedRegion {
        std::size_t start = g_armed_allocs.load(std::memory_order_relaxed);
        ArmedRegion() {
            t_armed = true;
        }
        [[nodiscard]] std::size_t finish() const {
            t_armed = false;
            return g_armed_allocs.load(std::memory_order_relaxed) - start;
        }
    };

    ContextHandler noop_handler() {
        return [](RequestContext ctx) -> AsyncResponse { co_return ctx.ok(""); };
    }
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
    const std::size_t rounded = (size + a - 1) / a * a;
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

namespace {
    // Stack-backed arena — mirrors the real RequestArena and never reaches
    // operator new. A size-only monotonic_buffer_resource would pull its first
    // block from new_delete_resource INSIDE the armed region. Do NOT
    // "simplify" to the size-only ctor (same trap as the PR 1 gate).
    struct StackArena {
        std::array<std::byte, 8192> buf{};
        std::pmr::monotonic_buffer_resource res{buf.data(), buf.size()};
        std::pmr::polymorphic_allocator<> alloc{&res};
    };
}  // namespace

TEST(RoutingAllocationGateTest, ExactMatchIsAllocationFree) {
    RouteRegistry reg;
    reg.add_route(HttpMethod::get, "/users/list", noop_handler());
    ASSERT_TRUE(reg.freeze().empty());
    StackArena arena;

    ArmedRegion region;
    auto resolved            = reg.find_route(HttpMethod::get, "/users/list", arena.alloc);
    const std::size_t allocs = region.finish();

    EXPECT_EQ(allocs, 0u) << "exact find_route touched the global heap";
    ASSERT_TRUE(resolved.is_success());
}

TEST(RoutingAllocationGateTest, ParametricMatchWithTrailingSlashIsAllocationFree) {
    RouteRegistry reg;
    reg.add_route(HttpMethod::get, "/users/{id}/posts/{post_id}", noop_handler());
    ASSERT_TRUE(reg.freeze().empty());
    StackArena arena;

    ArmedRegion region;
    auto resolved = reg.find_route(HttpMethod::get, "/users/12345/posts/678/", arena.alloc);
    const std::size_t allocs = region.finish();

    EXPECT_EQ(allocs, 0u) << "parametric find_route touched the global heap";
    ASSERT_TRUE(resolved.is_success());
    ASSERT_EQ(resolved.value().path_params.size(), 2u);
    EXPECT_EQ(resolved.value().path_params[0].second, "12345");  // zero-copy capture
    EXPECT_EQ(resolved.value().path_params[1].second, "678");
}

TEST(RoutingAllocationGateTest, PercentDecodedCaptureStaysInTheArena) {
    RouteRegistry reg;
    reg.add_route(HttpMethod::get, "/files/{name}", noop_handler());
    ASSERT_TRUE(reg.freeze().empty());
    StackArena arena;

    ArmedRegion region;
    auto resolved            = reg.find_route(HttpMethod::get, "/files/report%202026", arena.alloc);
    const std::size_t allocs = region.finish();

    EXPECT_EQ(allocs, 0u) << "capture decode escaped the arena";
    ASSERT_TRUE(resolved.is_success());
    EXPECT_EQ(resolved.value().path_params[0].second, "report 2026");
}

TEST(RoutingAllocationGateTest, ObserverHookInvocationIsAllocationFree) {
    // The Server-wired fan-out lambdas capture one pointer (fits std::function
    // SBO); INVOKING them per request must never touch the global heap.
    // set_hooks itself runs once at setup() — build phase, heap is fine there.
    std::size_t calls = 0;
    const Router::Hooks hooks{
        .on_request             = [p = &calls](const RequestContext&) noexcept { ++*p; },
        .on_response            = [p = &calls](const RequestInfo&, const Response&) noexcept { ++*p; },
        .on_unhandled_exception = [p = &calls](std::exception_ptr) noexcept { ++*p; },
    };

    StackArena arena;
    Request req{Headers::owned(arena.alloc)};
    req.method = HttpMethod::get;
    req.target = "/ping";  // string literal — static storage, no alloc
    RequestContext ctx{std::move(req), arena.alloc};
    Response resp{arena.alloc};
    const RequestInfo info{ctx.method(), ctx.target()};

    ArmedRegion region;
    hooks.on_request(ctx);
    hooks.on_response(info, resp);
    const std::size_t allocs = region.finish();

    EXPECT_EQ(allocs, 0u) << "observer hook invocation touched the global heap";
    EXPECT_EQ(calls, 2u);
}
