#include <memory>
#include <string>
#include <vector>

#include <controller.hpp>
#include <gtest/gtest.h>
#include <response_factory.hpp>
#include <route_registry.hpp>

#include "routing_test_utils.hpp"

using namespace demiplane::http;
using http_routing_test::run_awaitable;

namespace {

    struct RequestId {
        int value = 0;
    };

    class MwController final : public HttpController {
    public:
        bool handler_ran = false;

        void configure_routes() override {
            Get("/traced", &MwController::traced);
            Get("/id", &MwController::with_id);
        }

    private:
        AsyncResponse traced(RequestContext ctx) {
            handler_ran = true;
            co_return ctx.ok("handled");
        }
        static AsyncResponse with_id(RequestContext ctx) {
            const auto* id = ctx.get<RequestId>();
            co_return ctx.ok(id ? std::to_string(id->value) : "none");
        }
    };

    Middleware tracer(std::vector<std::string>& trace, std::string tag) {
        return [&trace, tag = std::move(tag)](RequestContext ctx, const NextHandler& next) -> AsyncResponse {
            trace.push_back(tag + ":before");
            auto r = co_await next(std::move(ctx));
            trace.push_back(tag + ":after");
            co_return r;
        };
    }

}  // namespace

class MiddlewareTest : public http_routing_test::RoutingTestBase {
protected:
    RouteRegistry registry_;
    std::shared_ptr<MwController> ctrl_ = std::make_shared<MwController>();

    void bake_and_freeze() {
        detail::ControllerBaker::bake_into(registry_, ctrl_, "");
        ASSERT_TRUE(registry_.freeze().empty());
    }

    Response invoke(const std::string& path) {
        auto resolved = registry_.find_route(HttpMethod::get, path, alloc_);
        EXPECT_TRUE(resolved.is_success());
        return run_awaitable((*resolved.value().handler)(make_ctx(HttpMethod::get, path)));
    }
};

TEST_F(MiddlewareTest, FirstAddedIsOutermost) {
    std::vector<std::string> trace;
    ctrl_->add_middleware(tracer(trace, "A"));
    ctrl_->add_middleware(tracer(trace, "B"));
    bake_and_freeze();

    EXPECT_EQ(*invoke("/traced").body.buffered_view(), "handled");
    const std::vector<std::string> expected{"A:before", "B:before", "B:after", "A:after"};
    EXPECT_EQ(trace, expected);
}

TEST_F(MiddlewareTest, ShortCircuitSkipsHandler) {
    ctrl_->add_middleware([](RequestContext ctx, const NextHandler& next) -> AsyncResponse {
        if (!ctx.header("Authorization"))
            co_return ResponseFactory::unauthorized();
        co_return co_await next(std::move(ctx));
    });
    bake_and_freeze();

    EXPECT_EQ(invoke("/traced").status, HttpStatus::unauthorized);
    EXPECT_FALSE(ctrl_->handler_ran);
}

TEST_F(MiddlewareTest, PostHandlerResponseMutation) {
    ctrl_->add_middleware([](RequestContext ctx, const NextHandler& next) -> AsyncResponse {
        auto r = co_await next(std::move(ctx));
        r.add_header("X-Traced", "1");
        co_return r;
    });
    bake_and_freeze();

    const Response r = invoke("/traced");
    ASSERT_TRUE(r.headers.get("X-Traced").has_value());
    EXPECT_EQ(*r.headers.get("X-Traced"), "1");
}

TEST_F(MiddlewareTest, BagFlowsFromMiddlewareToHandler) {
    ctrl_->add_middleware([](RequestContext ctx, const NextHandler& next) -> AsyncResponse {
        ctx.set(RequestId{42});
        co_return co_await next(std::move(ctx));
    });
    bake_and_freeze();

    EXPECT_EQ(*invoke("/id").body.buffered_view(), "42");
}

TEST_F(MiddlewareTest, AddBasicMiddlewareAppendsInOrder) {
    std::vector<std::string> trace;
    add_basic_middleware(*ctrl_, tracer(trace, "log"), tracer(trace, "auth"));
    bake_and_freeze();

    (void)invoke("/traced");
    const std::vector<std::string> expected{"log:before", "auth:before", "auth:after", "log:after"};
    EXPECT_EQ(trace, expected);
}
