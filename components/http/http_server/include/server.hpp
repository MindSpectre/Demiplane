
#pragma once

#include <atomic>
#include <memory>
#include <vector>

#include "aliases.hpp"
#include "controller.hpp"
#include "route_registry.hpp"
#include <boost/asio/io_context.hpp>

namespace demiplane::http {

    class Server {
    public:
        explicit Server(std::size_t threads = 1);
        ~Server();

        // Controller management - Server only handles processing
        template <typename ControllerT>
        void add_controller(std::shared_ptr<ControllerT> controller);

        // Middleware
        void use_middleware(Middleware middleware);

        // Callbacks
        void on_server_start(ServerCallback callback);
        void on_server_stop(ServerCallback callback);
        void on_request(RequestCallback callback);
        void on_response(ResponseCallback callback);
        void on_error(ErrorCallback callback);

        // Server lifecycle
        void listen(uint16_t port);
        void run();
        void stop();

    private:
        boost::asio::io_context ioc_;
        std::size_t thread_count_;
        RouteRegistry registry_; // Single registry for all routes
        std::vector<Middleware> middlewares_;
        std::vector<std::shared_ptr<HttpController>> controllers_;
        std::atomic<bool> running_{false};

        // Callbacks
        std::vector<ServerCallback> start_callbacks_;
        std::vector<ServerCallback> stop_callbacks_;
        std::vector<RequestCallback> request_callbacks_;
        std::vector<ResponseCallback> response_callbacks_;
        std::vector<ErrorCallback> error_callbacks_;

        boost::asio::awaitable<void> session(boost::asio::ip::tcp::socket socket) const;
        boost::asio::awaitable<Response> handle_request(Request request) const;

        void merge_controller_routes(HttpController* controller);
        static std::unordered_map<std::string, std::string> parse_query_params(const std::string& query);

        // Callback triggers
        void trigger_start_callbacks() const;
        void trigger_stop_callbacks() const;
        void trigger_request_callbacks(const Request& req) const;
        void trigger_response_callbacks(const Response& res) const;
        void trigger_error_callbacks(const std::exception& e) const;
    };

    template <typename ControllerT>
    void Server::add_controller(std::shared_ptr<ControllerT> controller) {
        static_assert(std::is_base_of_v<HttpController, ControllerT>, "Controller must inherit from HttpController");

        controllers_.push_back(controller);
        controller->configure_routes();
        controller->initialize();

        // Merge controller's routes into server's registry
        merge_controller_routes(controller.get());
    }

} // namespace demiplane::http
