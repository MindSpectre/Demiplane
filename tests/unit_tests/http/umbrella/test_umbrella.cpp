/**
 * PR 7 umbrella smoke test: this translation unit includes ONLY
 * <demiplane/http> and its target links ONLY Demiplane.Component.HTTP (plus
 * gtest) — exactly what a downstream consumer does. One construct per layer
 * proves the umbrella header reaches every layer's public API and that the
 * combined library propagates the include dirs and libs to satisfy it.
 *
 * Scope caveat: the layer aggregates PUBLIC-link one another, so most single
 * drops from the umbrella's LIBRARIES list are MASKED here. Measured against
 * the landed link graph: removing .Types, .Routing, .Drivers, .Listeners or
 * .Config still builds (each is re-provided transitively); only .Connection
 * (quic_connection.hpp) and .Server (attach_default_listeners.hpp) have no
 * re-provider and fail. Per-layer under-linking is caught by the per-layer
 * dotted test links instead (D4) — this target guards the umbrella's own
 * aggregation and the header list's completeness.
 */
#include <demiplane/http>
#include <memory_resource>
#include <string>

#include <boost/asio/io_context.hpp>
#include <gtest/gtest.h>

namespace {

    namespace http = demiplane::http;

    TEST(HttpUmbrella, TypesLayerIsReachable) {
        const auto response = http::ResponseFactory::ok("hello");
        EXPECT_EQ(response.status, http::HttpStatus::ok);
        ASSERT_TRUE(response.body.buffered_view().has_value());
        EXPECT_EQ(*response.body.buffered_view(), "hello");
    }

    TEST(HttpUmbrella, RoutingAndConnectionLayersAreReachable) {
        http::RouteRegistry registry;
        registry.add_route(http::HttpMethod::get, "/smoke", [](http::RequestContext ctx) -> http::AsyncResponse {
            co_return ctx.ok("pong");
        });
        EXPECT_TRUE(registry.freeze().empty());

        http::RequestArena arena{1024};
        EXPECT_TRUE(registry.find_route(http::HttpMethod::get, "/smoke", arena.allocator()).is_success());

        std::pmr::string arena_backed{"arena-backed", arena.allocator()};
        EXPECT_EQ(arena_backed, "arena-backed");
    }

    TEST(HttpUmbrella, DriversLayerIsReachable) {
        static_assert(http::Http11Driver::id() == http::Protocol::http1);
        static_assert(http::Http2Driver::id() == http::Protocol::http2);
        static_assert(http::Http3Driver::id() == http::Protocol::http3);
        const http::Http11Driver driver{http::Http11Config{}};
        static_cast<void>(driver);
    }

    TEST(HttpUmbrella, ListenersLayerIsReachable) {
        http::ConnectionTracker tracker;
        EXPECT_EQ(tracker.in_flight(), 0U);
    }

    TEST(HttpUmbrella, ConfigLayerIsReachable) {
        const auto cfg = http::ServerConfig::Builder{}.finalize();
        EXPECT_EQ(cfg.body_limit(), 16U * 1024U * 1024U);
        EXPECT_EQ(cfg.request_arena_size(), 8192U);
    }

    TEST(HttpUmbrella, ServerLayerIsReachable) {
        boost::asio::io_context ioc;
        const http::Server server{http::ServerConfig::Builder{}.finalize(), ioc.get_executor()};
        EXPECT_FALSE(server.is_running());
    }

}  // namespace
