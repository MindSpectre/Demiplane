#include <memory>
#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

#include <controller.hpp>
#include <group.hpp>
#include <route_registry.hpp>
#include <router.hpp>

#include "routing_test_utils.hpp"

using namespace demiplane::http;
using http_routing_test::run_awaitable;

namespace {

    class ApiController final : public HttpController {
    public:
        void configure_routes() override {
            Get("/users/{id}", &ApiController::get_user);
            Post("/users", &ApiController::create);
            Get("/boom", &ApiController::boom);
            Get("/u/{a}/p/{b}", &ApiController::two_params);
        }

    private:
        AsyncResponse get_user(RequestContext ctx) {
            co_return ctx.ok("user:" + std::to_string(ctx.path_param<int>("id").value_or(-1)));
        }
        AsyncResponse create(RequestContext ctx) {
            co_return ctx.created("made");
        }
        AsyncResponse boom(RequestContext) {
            throw std::runtime_error{"handler exploded"};
        }
        AsyncResponse two_params(RequestContext ctx) {
            co_return ctx.ok(ctx.path_param<std::string>("a").value_or("")
                             + ","
                             + ctx.path_param<std::string>("b").value_or(""));
        }
    };

}  // namespace

class RouterTest : public http_routing_test::RoutingTestBase {
protected:
    RouteRegistry registry_;
    std::vector<std::shared_ptr<HttpController>> controllers_;

    void SetUp() override {
        GroupBinding{registry_, controllers_, ""}.add_controller(
            std::make_shared<ApiController>());
        ASSERT_TRUE(registry_.freeze().empty());
    }
};

TEST_F(RouterTest, DispatchInvokesHandlerWithPathParams) {
    const Router router{registry_};
    const Response r = run_awaitable(router.dispatch(make_ctx(HttpMethod::get, "/users/42")));
    EXPECT_EQ(r.status, HttpStatus::ok);
    EXPECT_EQ(*r.body.buffered_view(), "user:42");
}

TEST_F(RouterTest, UnknownPathDispatchesTo404Response) {
    const Router router{registry_};
    const Response r = run_awaitable(router.dispatch(make_ctx(HttpMethod::get, "/missing")));
    EXPECT_EQ(r.status, HttpStatus::not_found);
}

TEST_F(RouterTest, WrongVerbDispatchesTo405WithAllowHeader) {
    const Router router{registry_};
    const Response r = run_awaitable(router.dispatch(make_ctx(HttpMethod::del, "/users")));
    EXPECT_EQ(r.status, HttpStatus::method_not_allowed);
    const auto allow = r.headers.get("Allow");
    ASSERT_TRUE(allow.has_value());
    EXPECT_NE(allow->find("POST"), std::string_view::npos);
}

TEST_F(RouterTest, QueryStringDoesNotConfuseRouting) {
    const Router router{registry_};
    const Response r = run_awaitable(
        router.dispatch(make_ctx(HttpMethod::get, "/users/7?verbose=1&x=%20")));
    EXPECT_EQ(*r.body.buffered_view(), "user:7");
}

TEST_F(RouterTest, HandlerExceptionsPropagateToCaller) {
    // The exception catch-all → 500 belongs to the h1 driver (PR 3, spec §6.3).
    const Router router{registry_};
    EXPECT_THROW(run_awaitable(router.dispatch(make_ctx(HttpMethod::get, "/boom"))),
                 std::runtime_error);
}

TEST_F(RouterTest, DispatchInjectsMultiplePathParams) {
    const Router router{registry_};
    const Response r = run_awaitable(router.dispatch(make_ctx(HttpMethod::get, "/u/11/p/22")));
    EXPECT_EQ(r.status, HttpStatus::ok);
    EXPECT_EQ(*r.body.buffered_view(), "11,22");
}

TEST_F(RouterTest, WrongVerbOnParametricRouteDispatchesTo405) {
    const Router router{registry_};
    const Response r = run_awaitable(router.dispatch(make_ctx(HttpMethod::del, "/users/42")));
    EXPECT_EQ(r.status, HttpStatus::method_not_allowed);  // GET /users/{id} exists, DELETE doesn't
}
