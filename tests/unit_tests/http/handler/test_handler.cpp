#include "controller.hpp"
#include "response_factory.hpp"
#include "server.hpp"
using namespace demiplane::http;

class UserController : public HttpController {
public:
    void configure_routes() override {
        // Beautiful method binding - no CRTP, no templates!
        Get("/users/{id}", &UserController::get_user);
        Post("/users", &UserController::create_user);
        Put("/users/{id}", &UserController::update_user);
        Delete("/users/{id}", &UserController::delete_user);
    }

private:
    Response get_user(RequestContext ctx) {
        auto user_id = ctx.path<int>("id");
        if (!user_id) {
            return ResponseFactory::bad_request("Invalid user ID");
        }

        std::string json =
            R"({"id":)" + std::to_string(*user_id) + R"(,"name":"User )" + std::to_string(*user_id) + R"("})";

        return ResponseFactory::json(json);
    }

    Response create_user(RequestContext ctx) {
        if (!ctx.is_json()) {
            return ResponseFactory::bad_request("Expected JSON content");
        }

        // Create user logic...
        return ResponseFactory::created(R"({"id":123,"status":"created"})");
    }

    Response update_user(RequestContext ctx) {
        auto user_id = ctx.path<int>("id");
        if (!user_id) {
            return ResponseFactory::bad_request("Invalid user ID");
        }

        // Update logic...
        return ResponseFactory::ok("Updated successfully");
    }

    Response delete_user(RequestContext ctx) {
        auto user_id = ctx.path<int>("id");
        if (!user_id) {
            return ResponseFactory::bad_request("Invalid user ID");
        }

        // Delete logic...
        return ResponseFactory::no_content();
    }
};

int main() {
    Server server(4);
    server.add_controller(std::make_shared<UserController>());
    server.listen(8080);
    server.run();
    return 0;
}
