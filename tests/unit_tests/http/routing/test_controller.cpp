#include <demiplane/gears>
#include <memory>
#include <stdexcept>
#include <string>

#include <controller.hpp>
#include <gtest/gtest.h>
#include <response_factory.hpp>
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
        static AsyncResponse list(RequestContext ctx) {
            co_return ctx.ok("list");
        }
        static AsyncResponse create(RequestContext ctx) {
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
        static AsyncResponse h(RequestContext ctx) {
            co_return ctx.ok("k");
        }
    };

    class OtherController final : public HttpController {
    public:
        void configure_routes() override {
        }
        AsyncResponse handler(RequestContext ctx) {  // NOLINT(readability-convert-member-functions-to-static)
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

    for (const auto m : {HttpMethod::get,
                         HttpMethod::post,
                         HttpMethod::put,
                         HttpMethod::patch,
                         HttpMethod::del,
                         HttpMethod::head,
                         HttpMethod::options}) {
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
    EXPECT_THROW(ctrl->add_middleware([](RequestContext ctx, const NextHandler& next) -> AsyncResponse {
        co_return co_await next(std::move(ctx));
    }),
                 std::logic_error);
}

TEST_F(ControllerTest, CrossControllerMemberThrowsAtBake) {
    auto ctrl = std::make_shared<CrossRegisteringController>();
    EXPECT_THROW(bake(ctrl), std::logic_error);
}

// ── Outcome→Response collapse ───────────────────────────────────────────────

namespace myapp {

    struct TeapotError {
        std::string blend;
    };

    // User-defined ADL conversion — lives next to the error type (spec §5.5).
    inline demiplane::http::Response to_http_response(const TeapotError& e) {
        return demiplane::http::ResponseFactory::custom(demiplane::http::HttpStatus::unprocessable_entity,
                                                        "teapot:" + e.blend);
    }

}  // namespace myapp

namespace {

    class OutcomeController final : public HttpController {
    public:
        void configure_routes() override {
            Get("/users/{id}", &OutcomeController::get_user);
            Post("/users", &OutcomeController::create_user);
            Get("/tea", [](RequestContext ctx) -> AsyncOutcome<Response, myapp::TeapotError> {
                if (ctx.query<bool>("brew").value_or(false))
                    co_return ctx.ok("brewing");
                co_return demiplane::gears::err(myapp::TeapotError{"earl-grey"});
            });
        }

    private:
        static AsyncOutcome<Response, NotFoundError> get_user(RequestContext ctx) {
            if (const auto id = ctx.path_param<int>("id"); id && *id == 42)
                co_return ctx.ok("user-42");
            co_return demiplane::gears::err(NotFoundError{"user", "?"});
        }

        static AsyncOutcome<Response, BadRequestError, ForbiddenError> create_user(RequestContext ctx) {
            const auto mode = ctx.query<std::string>("mode");
            if (mode == "bad")
                co_return demiplane::gears::err(BadRequestError{"bad mode"});
            if (mode == "forbidden")
                co_return demiplane::gears::err(ForbiddenError{"no"});
            co_return ctx.created("ok");
        }
    };

}  // namespace

class OutcomeControllerTest : public ControllerTest {
protected:
    void SetUp() override {
        auto ctrl = std::make_shared<OutcomeController>();
        bake(ctrl);
        ASSERT_TRUE(registry_.freeze().empty());
    }
};

TEST_F(OutcomeControllerTest, SuccessAlternativePassesThrough) {
    EXPECT_EQ(*invoke(HttpMethod::get, "/users/42").body.buffered_view(), "user-42");
}

TEST_F(OutcomeControllerTest, TypedErrorCollapsesViaAdl) {
    const Response r = invoke(HttpMethod::get, "/users/7");
    EXPECT_EQ(r.status, HttpStatus::not_found);
    EXPECT_EQ(*r.body.buffered_view(), "user ? not found");
}

TEST_F(OutcomeControllerTest, MultiErrorPackEachAlternative) {
    // make_ctx targets carry the query string; invoke() routes on the path.
    auto resolved = registry_.find_route(HttpMethod::post, "/users", alloc_);
    ASSERT_TRUE(resolved.is_success());
    const auto run = [&](const std::string& target) {
        return run_awaitable((*resolved.value().handler)(make_ctx(HttpMethod::post, target)));
    };
    EXPECT_EQ(run("/users?mode=bad").status, HttpStatus::bad_request);
    EXPECT_EQ(run("/users?mode=forbidden").status, HttpStatus::forbidden);
    EXPECT_EQ(run("/users").status, HttpStatus::created);
}

TEST_F(OutcomeControllerTest, UserDefinedErrorTypeViaLambda) {
    auto resolved = registry_.find_route(HttpMethod::get, "/tea", alloc_);
    ASSERT_TRUE(resolved.is_success());
    const Response err = run_awaitable((*resolved.value().handler)(make_ctx(HttpMethod::get, "/tea")));
    EXPECT_EQ(err.status, HttpStatus::unprocessable_entity);
    EXPECT_EQ(*err.body.buffered_view(), "teapot:earl-grey");

    const Response ok = run_awaitable((*resolved.value().handler)(make_ctx(HttpMethod::get, "/tea?brew=true")));
    EXPECT_EQ(ok.status, HttpStatus::ok);
    EXPECT_EQ(*ok.body.buffered_view(), "brewing");
}

// ── Compile-time gates (spec §8.3: missing to_http_response = build break) ──

namespace {
    struct NotRenderable {};

    using GoodPlain   = decltype([](RequestContext) -> AsyncResponse { co_return Response{}; });
    using GoodOutcome = decltype([](RequestContext) -> AsyncOutcome<Response, NotFoundError> { co_return Response{}; });
    using BadReturn   = decltype([](RequestContext) { return 42; });
    using BadError    = decltype([](RequestContext) -> AsyncOutcome<Response, NotRenderable> { co_return Response{}; });

    static_assert(detail::HasToHttpResponse<NotFoundError>);
    static_assert(detail::HasToHttpResponse<myapp::TeapotError>);
    static_assert(!detail::HasToHttpResponse<NotRenderable>);
    static_assert(IsRouteHandler<GoodPlain>);
    static_assert(IsRouteHandler<GoodOutcome>);
    static_assert(!IsRouteHandler<BadReturn>);
    static_assert(!IsRouteHandler<BadError>);  // error type without to_http_response

    // Mixed pack: one renderable + one not — concept must still reject.
    using BadMixedPack =
        decltype([](RequestContext) -> AsyncOutcome<Response, NotFoundError, NotRenderable> { co_return Response{}; });
    static_assert(!IsRouteHandler<BadMixedPack>);
}  // namespace
