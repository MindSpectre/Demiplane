#include <iostream>

#include "controller.hpp"
#include "gears_utils.hpp"
#include "response_factory.hpp"
#include "server.hpp"
#include <boost/asio/use_awaitable.hpp>
using namespace demiplane::http;

class UserController final : public HttpController {
public:
    void configure_routes() override {
        // Beautiful method binding - no CRTP, no templates!
        Get("/users/{id}", &UserController::get_user);
        Post("/users", &UserController::create_user);
        Put("/users/{id}", &UserController::update_user);
        Delete("/users/{id}", &UserController::delete_user);
    }

private:
    std::atomic<int> i = 0;
    // Instead of synchronous Response, use AsyncResponse
    AsyncResponse get_user(const RequestContext& ctx) {
        const auto user_id = ctx.path<int>("id");
        if (!user_id) {
            co_return ResponseFactory::bad_request("Invalid user ID");
        }
        int x{0};

        i.fetch_add(1, std::memory_order::relaxed);
        if (i.load() % 20 == 0) {
            std::cerr << "PRESSURE" << std::endl;
            co_return ResponseFactory::unauthorized("Pressure");
        }
        for (int i = 0; i < 10000; i++) {
            ++x;
        }
        demiplane::gears::enforce_non_const(this);
        const std::string json =
            R"({"id":)" + std::to_string(x) + R"(,"name":"User )" + std::to_string(*user_id) + R"("})";
        co_return ResponseFactory::json(json);
    }

    Response create_user(const RequestContext& ctx) {
        if (!ctx.is_json()) {
            return ResponseFactory::bad_request("Expected JSON content");
        }
        demiplane::gears::enforce_non_const(this);
        // Create user logic...
        return ResponseFactory::created(R"({"id":123,"status":"created"})");
    }

    Response update_user(const RequestContext& ctx) {
        if (const auto user_id = ctx.path<int>("id"); !user_id) {
            return ResponseFactory::bad_request("Invalid user ID");
        }
        demiplane::gears::enforce_non_const(this);
        // Update logic...
        return ResponseFactory::ok("Updated successfully");
    }

    Response delete_user(const RequestContext& ctx) {
        if (const auto user_id = ctx.path<int>("id"); !user_id) {
            return ResponseFactory::bad_request("Invalid user ID");
        }
        demiplane::gears::enforce_non_const(this);
        // Delete logic...
        return ResponseFactory::no_content();
    }
};

int main() {

    Server server(4);
    server.on_error([](const std::exception& e) { std::cerr << e.what() << std::endl; });
    server.on_request([](const Request& req) { std::cout << "REQ" << std::endl; });
    server.on_response([](const Response& res) { std::cout << "RES" << std::endl; });
    server.on_server_start([] { std::cout << "Server started" << std::endl; });
    server.add_controller(std::make_shared<UserController>());
    server.listen(8080);
    server.run();
    return 0;
}
