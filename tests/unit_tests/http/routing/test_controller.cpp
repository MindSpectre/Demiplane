#include <memory>
#include <stdexcept>
#include <string>

#include <gtest/gtest.h>

#include <controller.hpp>
#include <route_registry.hpp>

#include "routing_test_utils.hpp"

using namespace demiplane::http;
using http_routing_test::run_awaitable;

namespace {

    class PlainController final : public HttpController {
    public:
        int configure_calls = 0;

        void configure_routes() override {
            ++configure_calls;
            Get("/users", &PlainController::list);
            Post("/users", &PlainController::create);
        }

        // Public so a test can attempt late registration after bake.
        void late_register() {
            Get("/late", &PlainController::list);
        }

    private:
        AsyncResponse list(RequestContext ctx) {
            co_return ctx.ok("list");
        }
        AsyncResponse create(RequestContext ctx) {
            co_return ctx.created("made");
        }
    };

    AsyncResponse free_handler(RequestContext ctx) {
        co_return ctx.ok("free");
    }

    class KitchenSinkController final : public HttpController {
    public:
        void configure_routes() override {
            Get("/k", &KitchenSinkController::h);
            Post("/k", &KitchenSinkController::h);
            Put("/k", &KitchenSinkController::h);
            Patch("/k", &KitchenSinkController::h);
            Delete("/k", &KitchenSinkController::h);
            Head("/k", &KitchenSinkController::h);
            Options("/k", &KitchenSinkController::h);
            Get("/lambda", [](RequestContext ctx) -> AsyncResponse { co_return ctx.ok("lambda"); });
            Get("/free", &free_handler);
        }

    private:
        AsyncResponse h(RequestContext ctx) {
            co_return ctx.ok("k");
        }
    };

    class OtherController final : public HttpController {
    public:
        void configure_routes() override {}
        AsyncResponse handler(RequestContext ctx) {
            co_return ctx.ok("other");
        }
    };

    class CrossRegisteringController final : public HttpController {
    public:
        void configure_routes() override {
            // Member of a DIFFERENT controller type — must throw at bake.
            Get("/cross", &OtherController::handler);
        }
    };

}  // namespace

class ControllerTest : public http_routing_test::RoutingTestBase {
protected:
    RouteRegistry registry_;

    void bake(const std::shared_ptr<HttpController>& ctrl, const std::string_view prefix = "") {
        detail::ControllerBaker::bake_into(registry_, ctrl, prefix);
    }

    Response invoke(const HttpMethod m, const std::string& path) {
        auto resolved = registry_.find_route(m, path, alloc_);
        EXPECT_TRUE(resolved.is_success()) << "no route for " << path;
        auto ctx = make_ctx(m, path);
        for (const auto& [n, v] : resolved.value().path_params)
            ctx.set_path_param(n, v);
        return run_awaitable((*resolved.value().handler)(std::move(ctx)));
    }
};

TEST_F(ControllerTest, MemberHandlersBakeAndDispatch) {
    auto ctrl = std::make_shared<PlainController>();
    bake(ctrl);
    ASSERT_TRUE(registry_.freeze().empty());

    EXPECT_EQ(*invoke(HttpMethod::get, "/users").body.buffered_view(), "list");
    EXPECT_EQ(invoke(HttpMethod::post, "/users").status, HttpStatus::created);
}

TEST_F(ControllerTest, ConfigureRoutesRunsExactlyOnce) {
    auto ctrl = std::make_shared<PlainController>();
    bake(ctrl);
    EXPECT_EQ(ctrl->configure_calls, 1);
}

TEST_F(ControllerTest, AllSevenVerbsPlusCallables) {
    auto ctrl = std::make_shared<KitchenSinkController>();
    bake(ctrl);
    ASSERT_TRUE(registry_.freeze().empty());

    for (const auto m : {HttpMethod::get, HttpMethod::post, HttpMethod::put, HttpMethod::patch,
                         HttpMethod::del, HttpMethod::head, HttpMethod::options}) {
        EXPECT_TRUE(registry_.find_route(m, "/k", alloc_).is_success());
    }
    EXPECT_EQ(*invoke(HttpMethod::get, "/lambda").body.buffered_view(), "lambda");
    EXPECT_EQ(*invoke(HttpMethod::get, "/free").body.buffered_view(), "free");
}

TEST_F(ControllerTest, PrefixAppliedAtBake) {
    auto ctrl = std::make_shared<PlainController>();
    bake(ctrl, "/api/v1");
    ASSERT_TRUE(registry_.freeze().empty());
    EXPECT_TRUE(registry_.find_route(HttpMethod::get, "/api/v1/users", alloc_).is_success());
    EXPECT_TRUE(registry_.find_route(HttpMethod::get, "/users", alloc_).is_error());
}

TEST_F(ControllerTest, DoubleBakeThrows) {
    auto ctrl = std::make_shared<PlainController>();
    bake(ctrl);
    EXPECT_THROW(bake(ctrl), std::logic_error);
}

TEST_F(ControllerTest, LateRegistrationThrows) {
    auto ctrl = std::make_shared<PlainController>();
    bake(ctrl);
    EXPECT_THROW(ctrl->late_register(), std::logic_error);
}

TEST_F(ControllerTest, AddMiddlewareAfterBakeThrows) {
    auto ctrl = std::make_shared<PlainController>();
    bake(ctrl);
    EXPECT_THROW(ctrl->add_middleware(
                     [](RequestContext ctx, const NextHandler& next) -> AsyncResponse {
                         co_return co_await next(std::move(ctx));
                     }),
                 std::logic_error);
}

TEST_F(ControllerTest, CrossControllerMemberThrowsAtBake) {
    auto ctrl = std::make_shared<CrossRegisteringController>();
    EXPECT_THROW(bake(ctrl), std::logic_error);
}
