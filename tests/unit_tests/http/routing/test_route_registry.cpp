#include <stdexcept>
#include <string>

#include <gtest/gtest.h>

#include <route_registry.hpp>

#include "routing_test_utils.hpp"

using namespace demiplane::http;
using http_routing_test::run_awaitable;

namespace {
    ContextHandler tag_handler(std::string tag) {
        return [tag = std::move(tag)](RequestContext ctx) -> AsyncResponse {
            co_return ctx.ok(tag);
        };
    }
}

// ── Registration / freeze / conflicts ──────────────────────────────────────

TEST(RouteRegistryTest, FreezeWithoutConflicts) {
    RouteRegistry reg;
    reg.add_route(HttpMethod::get, "/users", tag_handler("u-get"));
    reg.add_route(HttpMethod::post, "/users", tag_handler("u-post"));
    reg.add_route(HttpMethod::get, "/health", tag_handler("h"));
    EXPECT_FALSE(reg.is_frozen());
    EXPECT_TRUE(reg.freeze().empty());
    EXPECT_TRUE(reg.is_frozen());
}

TEST(RouteRegistryTest, DuplicateIsRecordedNotThrown) {
    RouteRegistry reg;
    reg.add_route(HttpMethod::get, "/users", tag_handler("a"));
    reg.add_route(HttpMethod::get, "/users", tag_handler("b"));  // duplicate — no throw
    const auto conflicts = reg.freeze();
    ASSERT_EQ(conflicts.size(), 1u);
    EXPECT_EQ(conflicts[0].method, HttpMethod::get);
    EXPECT_EQ(conflicts[0].path, "/users");
}

TEST(RouteRegistryTest, DuplicateDetectedAcrossNormalization) {
    RouteRegistry reg;  // default: collapse_trailing_slash
    reg.add_route(HttpMethod::get, "/users", tag_handler("a"));
    reg.add_route(HttpMethod::get, "/users/", tag_handler("b"));  // same normalized path
    EXPECT_EQ(reg.freeze().size(), 1u);
}

TEST(RouteRegistryTest, RegistrationAfterFreezeThrows) {
    RouteRegistry reg;
    reg.add_route(HttpMethod::get, "/a", tag_handler("a"));
    (void)reg.freeze();
    EXPECT_THROW(reg.add_route(HttpMethod::get, "/b", tag_handler("b")), std::logic_error);
}

TEST(RouteRegistryTest, InvalidRegistrationsThrow) {
    RouteRegistry reg;
    EXPECT_THROW(reg.add_route(HttpMethod::unknown, "/a", tag_handler("a")), std::invalid_argument);
    EXPECT_THROW(reg.add_route(HttpMethod::get, "/a", ContextHandler{}), std::invalid_argument);
    EXPECT_THROW(reg.add_route(HttpMethod::get, "no-slash", tag_handler("a")), std::invalid_argument);
    EXPECT_THROW(reg.add_route(HttpMethod::get, "", tag_handler("a")), std::invalid_argument);
}

TEST(RouteRegistryTest, LiteralSegmentRejectsPercentAndBraces) {
    RouteRegistry reg;
    EXPECT_THROW(reg.add_route(HttpMethod::get, "/a%20b", tag_handler("a")), std::invalid_argument);
    EXPECT_THROW(reg.add_route(HttpMethod::get, "/a{b/c", tag_handler("a")), std::invalid_argument);
    EXPECT_THROW(reg.add_route(HttpMethod::get, "/a}b", tag_handler("a")), std::invalid_argument);
}

TEST(RouteRegistryTest, ParamNameValidation) {
    RouteRegistry reg;
    EXPECT_THROW(reg.add_route(HttpMethod::get, "/{}", tag_handler("a")), std::invalid_argument);
    EXPECT_THROW(reg.add_route(HttpMethod::get, "/{id}/x/{id}", tag_handler("a")), std::invalid_argument);
    // valid parametric registration is fine
    reg.add_route(HttpMethod::get, "/{id}/x/{other}", tag_handler("a"));
    EXPECT_TRUE(reg.freeze().empty());
}

TEST(RouteRegistryTest, SameShapeDifferentParamNamesIsConflict) {
    RouteRegistry reg;
    reg.add_route(HttpMethod::get, "/u/{a}", tag_handler("a"));
    reg.add_route(HttpMethod::post, "/u/{b}", tag_handler("b"));  // shape-equal, names differ
    const auto conflicts = reg.freeze();
    ASSERT_EQ(conflicts.size(), 1u);
    EXPECT_EQ(conflicts[0].method, HttpMethod::post);
}

TEST(RouteRegistryTest, SameTemplateTwoMethodsIsNotAConflict) {
    RouteRegistry reg;
    reg.add_route(HttpMethod::get, "/u/{id}", tag_handler("g"));
    reg.add_route(HttpMethod::post, "/u/{id}", tag_handler("p"));
    EXPECT_TRUE(reg.freeze().empty());
}

TEST(RouteRegistryTest, DifferentShapeParametricRoutesCoexist) {
    RouteRegistry reg;
    reg.add_route(HttpMethod::get, "/u/{id}", tag_handler("a"));
    reg.add_route(HttpMethod::get, "/u/{id}/posts", tag_handler("b"));  // different shape
    EXPECT_TRUE(reg.freeze().empty());
}

// ── join_path ───────────────────────────────────────────────────────────────

TEST(JoinPathTest, Battery) {
    EXPECT_EQ(join_path("", "/users"), "/users");
    EXPECT_EQ(join_path("/api/v1", "/users"), "/api/v1/users");
    EXPECT_EQ(join_path("/api/", "/users"), "/api/users");
    EXPECT_EQ(join_path("/api", "/"), "/api");
    EXPECT_EQ(join_path("", "/"), "/");
    EXPECT_EQ(join_path("/api", "users"), "/api/users");
    EXPECT_EQ(join_path("/", "/x"), "/x");
}
