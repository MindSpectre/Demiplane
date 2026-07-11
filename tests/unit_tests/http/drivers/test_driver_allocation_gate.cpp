#include <array>
#include <atomic>
#include <cstddef>
#include <memory>
#include <memory_resource>
#include <new>
#include <string>
#include <tuple>
#include <vector>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/use_future.hpp>
#include <boost/beast/http.hpp>
#include <controller.hpp>
#include <group.hpp>
#include <gtest/gtest.h>
#include <http11_driver.hpp>
#include <middleware.hpp>
#include <route_registry.hpp>
#include <router.hpp>

using namespace demiplane::http;
namespace http = boost::beast::http;

// ── Armed global operator new/delete (same mechanism as the PR1/PR2 gates) ──
namespace {
    std::atomic<std::size_t> g_allocs{0};
    thread_local bool t_armed = false;

    struct ArmedRegion {
        std::size_t start = g_allocs.load(std::memory_order_relaxed);
        ArmedRegion() {
            t_armed = true;
        }
        [[nodiscard]] std::size_t finish() const {
            t_armed = false;
            return g_allocs.load(std::memory_order_relaxed) - start;
        }
    };
}  // namespace

void* operator new(std::size_t n) {
    if (t_armed)
        g_allocs.fetch_add(1, std::memory_order_relaxed);
    if (void* p = std::malloc(n))
        return p;
    throw std::bad_alloc{};
}
void* operator new(std::size_t n, std::align_val_t a) {
    if (t_armed)
        g_allocs.fetch_add(1, std::memory_order_relaxed);
    const auto al = static_cast<std::size_t>(a);
    if (void* p = std::aligned_alloc(al, (n + al - 1) / al * al))
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
    // Stack arena — never reaches operator new (same trap-avoidance as PR1/PR2:
    // a size-only monotonic_buffer_resource would pull its first block from
    // new_delete_resource INSIDE the armed region).
    struct StackArena {
        std::array<std::byte, 16384> buf{};
        std::pmr::monotonic_buffer_resource res{buf.data(), buf.size()};
        std::pmr::polymorphic_allocator<> alloc{&res};
    };

    class GateController final : public HttpController {
    public:
        void configure_routes() override {
            Get("/empty", &GateController::empty);    // no body  -> baseline
            Get("/withbody", &GateController::body);  // 1 user string
        }

    private:
        AsyncResponse empty(RequestContext ctx) {
            demiplane::gears::force_non_const(this);
            co_return ctx.no_content();
        }
        AsyncResponse body(RequestContext ctx) {
            demiplane::gears::force_non_const(this);
            co_return ctx.ok(std::string(64, 'B'));  // >SSO: exactly one heap string
        }
    };

    // Build a parsed GET request of the driver's exact type, with `target`,
    // OUTSIDE any armed region.
    detail::Http11Request make_get(const std::pmr::polymorphic_allocator<> arena, const std::string_view target) {
        // Fields as well as body come from the arena — Http11Request's fields are
        // BeastFields (pmr). A default-constructed pmr allocator would silently
        // fall back to new_delete and every header node would hit the global heap.
        std::pmr::polymorphic_allocator<char> arena_alloc{arena.resource()};
        detail::Http11Request req{
            std::piecewise_construct, std::forward_as_tuple(arena_alloc), std::forward_as_tuple(arena_alloc)};
        req.method(http::verb::get);
        req.target(target);
        req.version(11);
        return req;
    }

    // Measure global allocs across build_request_context -> dispatch ->
    // make_beast_response. The Beast translation used to be excluded because it
    // allocated one std::allocator node per header by construction; its fields
    // are now arena-backed, so it is inside the armed region and gated too.
    std::size_t measure(Router& router, std::string target, StackArena& arena) {
        boost::asio::io_context ioc;
        auto fut = boost::asio::co_spawn(
            ioc,
            [&]() -> boost::asio::awaitable<std::size_t> {
                detail::Http11Request req = make_get(arena.alloc, target);  // before arming
                ArmedRegion region;
                RequestContext ctx = detail::build_request_context(req, arena.alloc);
                Response resp      = co_await router.dispatch(std::move(ctx));
                (void)detail::make_beast_response(resp);
                co_return region.finish();
            },
            boost::asio::use_future);
        ioc.run();
        return fut.get();
    }

    Router freeze_router(RouteRegistry& reg,
                         std::vector<std::shared_ptr<HttpController>>& sink,
                         const std::vector<Middleware>& mws = {}) {
        auto ctrl = std::make_shared<GateController>();
        for (const auto& mw : mws)
            ctrl->add_middleware(mw);
        GroupBinding{reg, sink, ""}.add_controller(ctrl);
        (void)reg.freeze();
        return Router{reg};
    }
}  // namespace

TEST(DriverAllocationGateTest, ArenaHeaderMutationAddsNoGlobalHeap) {
    // Two middleware variants with the SAME coroutine-frame structure (each user
    // middleware that co_awaits next is one heap frame). The ONLY difference is
    // whether the post-handler middleware mutates two response headers. A
    // bare-vs-middleware comparison would be confounded by the middleware's own
    // frame; this isolates the arena-header-mutation cost.
    Middleware passthrough = [](RequestContext ctx, const NextHandler& next) -> AsyncResponse {
        co_return co_await next(std::move(ctx));
    };
    Middleware add_headers = [](RequestContext ctx, const NextHandler& next) -> AsyncResponse {
        Response r = co_await next(std::move(ctx));
        r.add_header("X-A", "1");
        r.add_header("X-B", "2");  // arena-backed — must not touch the global heap
        co_return r;
    };

    RouteRegistry reg_pass;
    std::vector<std::shared_ptr<HttpController>> sink_pass;
    Router r_pass = freeze_router(reg_pass, sink_pass, {passthrough});
    StackArena a_pass;
    const std::size_t passthrough_allocs = measure(r_pass, "/empty", a_pass);

    RouteRegistry reg_add;
    std::vector<std::shared_ptr<HttpController>> sink_add;
    Router r_add = freeze_router(reg_add, sink_add, {add_headers});
    StackArena a_add;
    const std::size_t with_headers = measure(r_add, "/empty", a_add);

    EXPECT_EQ(with_headers, passthrough_allocs) << "post-handler arena header mutation hit the global heap ("
                                                << with_headers << " vs " << passthrough_allocs << ")";
}

TEST(DriverAllocationGateTest, OneUserBodyStringIsExactlyOneAlloc) {
    RouteRegistry reg_base;
    std::vector<std::shared_ptr<HttpController>> sink_base;
    Router r_base = freeze_router(reg_base, sink_base);
    StackArena a_base;
    const std::size_t baseline = measure(r_base, "/empty", a_base);

    RouteRegistry reg_body;
    std::vector<std::shared_ptr<HttpController>> sink_body;
    Router r_body = freeze_router(reg_body, sink_body);
    StackArena a_body;
    const std::size_t with_body = measure(r_body, "/withbody", a_body);

    EXPECT_EQ(with_body, baseline + 1) << "framework added allocations beyond the single user-body string ("
                                       << with_body << " vs " << baseline << "+1)";
}
