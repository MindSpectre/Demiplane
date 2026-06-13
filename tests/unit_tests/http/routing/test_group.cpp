#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include <controller.hpp>
#include <group.hpp>
#include <route_registry.hpp>

#include "routing_test_utils.hpp"

using namespace demiplane::http;

namespace {

    class UsersController final : public HttpController {
    public:
        void configure_routes() override {
            Get("/users", &UsersController::list);
            Get("/users/{id}", &UsersController::one);
        }

    private:
        AsyncResponse list(RequestContext ctx) { co_return ctx.ok("users"); }
        AsyncResponse one(RequestContext ctx) { co_return ctx.ok("one"); }
    };

    class HealthController final : public HttpController {
    public:
        void configure_routes() override {
            Get("/health", &HealthController::ping);
        }

    private:
        AsyncResponse ping(RequestContext ctx) { co_return ctx.ok("ok"); }
    };

    class ClashingController final : public HttpController {
    public:
        void configure_routes() override {
            Get("/users", &ClashingController::also_users);
        }

    private:
        AsyncResponse also_users(RequestContext ctx) { co_return ctx.ok("clash"); }
    };

}  // namespace

class GroupTest : public http_routing_test::RoutingTestBase {
protected:
    RouteRegistry registry_;
    std::vector<std::shared_ptr<HttpController>> controllers_;

    GroupBinding root() { return GroupBinding{registry_, controllers_, ""}; }
};

TEST_F(GroupTest, PrefixAppliedToEveryRoute) {
    root().in_group("/api/v1").add_controller(std::make_shared<UsersController>());
    ASSERT_TRUE(registry_.freeze().empty());

    EXPECT_TRUE(registry_.find_route(HttpMethod::get, "/api/v1/users", alloc_).is_success());
    EXPECT_TRUE(registry_.find_route(HttpMethod::get, "/api/v1/users/7", alloc_).is_success());
    EXPECT_TRUE(registry_.find_route(HttpMethod::get, "/users", alloc_).is_error());
}

TEST_F(GroupTest, NestedGroupsConcatenatePrefixes) {
    root().in_group("/api").in_group("/v2").add_controller(std::make_shared<UsersController>());
    ASSERT_TRUE(registry_.freeze().empty());
    EXPECT_TRUE(registry_.find_route(HttpMethod::get, "/api/v2/users", alloc_).is_success());
}

TEST_F(GroupTest, MultipleControllersOneGroupAndChaining) {
    root()
        .in_group("/api")
        .add_controller(std::make_shared<UsersController>())
        .add_controller(std::make_shared<HealthController>());
    ASSERT_TRUE(registry_.freeze().empty());

    EXPECT_TRUE(registry_.find_route(HttpMethod::get, "/api/users", alloc_).is_success());
    EXPECT_TRUE(registry_.find_route(HttpMethod::get, "/api/health", alloc_).is_success());
    EXPECT_EQ(controllers_.size(), 2u);  // sink records ownership for PR 5 lifecycle
}

TEST_F(GroupTest, EmptyPrefixMountsAtRoot) {
    root().add_controller(std::make_shared<HealthController>());
    ASSERT_TRUE(registry_.freeze().empty());
    EXPECT_TRUE(registry_.find_route(HttpMethod::get, "/health", alloc_).is_success());
}

TEST_F(GroupTest, CrossControllerConflictSurfacesAtFreeze) {
    auto api = root().in_group("/api");
    api.add_controller(std::make_shared<UsersController>());
    api.add_controller(std::make_shared<ClashingController>());  // also GET /api/users

    const auto conflicts = registry_.freeze();
    ASSERT_EQ(conflicts.size(), 1u);
    EXPECT_EQ(conflicts[0].method, HttpMethod::get);
    EXPECT_EQ(conflicts[0].path, "/api/users");
}

TEST_F(GroupTest, InGroupDoesNotMutateParent) {
    auto api = root().in_group("/api");
    api.in_group("/v1").add_controller(std::make_shared<UsersController>());
    api.in_group("/v2").add_controller(std::make_shared<HealthController>());
    ASSERT_TRUE(registry_.freeze().empty());

    EXPECT_EQ(api.prefix(), "/api");  // descents did not mutate the parent
    EXPECT_TRUE(registry_.find_route(HttpMethod::get, "/api/v1/users", alloc_).is_success());
    EXPECT_TRUE(registry_.find_route(HttpMethod::get, "/api/v2/health", alloc_).is_success());
}

TEST_F(GroupTest, SameControllerInTwoGroupsThrows) {
    auto users = std::make_shared<UsersController>();
    root().in_group("/a").add_controller(users);
    EXPECT_THROW(root().in_group("/b").add_controller(users), std::logic_error);
}
