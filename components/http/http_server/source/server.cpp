#include "server.hpp"

#include <iostream>
#include <sstream>
#include <thread>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/beast/core.hpp>
#include <demiplane/gears>
#include <demiplane/scroll>

#include "request_context.hpp"
#include "response_factory.hpp"

namespace demiplane::http {
    namespace beast = boost::beast;
    namespace asio = boost::asio;
    using tcp = asio::ip::tcp;

    Server::Server(const std::size_t threads)
        : ioc_(static_cast<int>(threads)),
          thread_count_(threads) {
        COMPONENT_LOG_INF() << "Server was created with " << thread_count_ << " threads";
    }

    Server::~Server() {
        COMPONENT_LOG_INF() << "Server was shut down";
        if (running_) {
            stop();
        }
    }

    void Server::use_middleware(Middleware middleware) {
        middlewares_.push_back(std::move(middleware));
    }

    void Server::on_server_start(ServerCallback callback) {
        start_callbacks_.push_back(std::move(callback));
    }

    void Server::on_server_stop(ServerCallback callback) {
        stop_callbacks_.push_back(std::move(callback));
    }

    void Server::on_request(RequestCallback callback) {
        request_callbacks_.push_back(std::move(callback));
    }

    void Server::on_response(ResponseCallback callback) {
        response_callbacks_.push_back(std::move(callback));
    }

    void Server::on_error(ErrorCallback callback) {
        error_callbacks_.push_back(std::move(callback));
    }

    // Async callback registration
    void Server::on_server_start_async(AsyncServerCallback callback) {
        async_start_callbacks_.push_back(std::move(callback));
    }

    void Server::on_server_stop_async(AsyncServerCallback callback) {
        async_stop_callbacks_.push_back(std::move(callback));
    }

    void Server::on_request_async(AsyncRequestCallback callback) {
        async_request_callbacks_.push_back(std::move(callback));
    }

    void Server::on_response_async(AsyncResponseCallback callback) {
        async_response_callbacks_.push_back(std::move(callback));
    }

    void Server::on_error_async(AsyncErrorCallback callback) {
        async_error_callbacks_.push_back(std::move(callback));
    }


    void Server::listen(uint16_t port) {
        COMPONENT_LOG_INF() << "Server started listening on port " << port;
        running_ = true;
        trigger_start_callbacks();

        asio::co_spawn(
            ioc_,
            [this, port]() -> asio::awaitable<void> {
                const tcp::endpoint ep{tcp::v4(), port};
                tcp::acceptor acceptor(co_await asio::this_coro::executor, ep);

                while (running_) {
                    beast::error_code ec;
                    tcp::socket sock = co_await acceptor.async_accept(asio::redirect_error(asio::use_awaitable, ec));

                    if (ec) {
                        if (ec == asio::error::operation_aborted) {
                            break;
                        }
                        continue;
                    }

                    asio::co_spawn(co_await asio::this_coro::executor, session(std::move(sock)), asio::detached);
                }
            },
            asio::detached);
    }

    void Server::run() {
        asio::signal_set signals(ioc_, SIGINT, SIGTERM);
        signals.async_wait([this](auto, auto) {
            stop();
        });

        std::vector<std::thread> threads;
        for (std::size_t i = 1; i < thread_count_; ++i) {
            threads.emplace_back([this] {
                ioc_.run();
            });
        }
        COMPONENT_LOG_INF() << "Server started";
        ioc_.run();

        for (auto& t : threads) {
            t.join();
        }

        trigger_stop_callbacks();
        COMPONENT_LOG_INF() << "Server stopped";
        for (const auto& controller : controllers_) {
            controller->shutdown();
        }
        COMPONENT_LOG_INF() << "Controllers stopped";
    }

    void Server::stop() {
        COMPONENT_LOG_DBG() << "Server stop initiated";
        running_ = false;
        ioc_.stop();
    }

    asio::awaitable<void> Server::session(tcp::socket socket) const {
        beast::tcp_stream stream(std::move(socket));

        try {
            bool keep_alive;
            beast::flat_buffer buffer;
            do {
                Request req;

                beast::error_code ec;
                co_await beast::http::async_read(stream, buffer, req, asio::redirect_error(asio::use_awaitable, ec));

                if (ec == beast::http::error::end_of_stream) {
                    break;
                }
                if (ec) {
                    co_return;
                }

                Response res = co_await handle_request(std::move(req));

                keep_alive = res.keep_alive();
                co_await beast::http::async_write(stream, res, asio::use_awaitable);
            }
            while (keep_alive);
        }
        catch (const std::exception& e) {
            trigger_error_callbacks(e);
        }

        beast::error_code ec;
        auto x = stream.socket().shutdown(tcp::socket::shutdown_send, ec);
        gears::unused_value(x);
    }

    AsyncResponse Server::handle_request(Request request) const {
        trigger_request_callbacks(request);
        auto target    = std::string(request.target());
        auto query_pos = target.find('?');
        auto path      = query_pos != std::string::npos ? target.substr(0, query_pos) : target;
        auto query     = query_pos != std::string::npos ? target.substr(query_pos + 1) : "";
        ContextHandler handler;
        std::unordered_map<std::string, std::string> path_params;
        try {
            auto [fst, snd] = registry_.find_handler(request.method(), path);
            handler         = std::move(fst);
            path_params     = std::move(snd);
        }
        catch ([[maybe_unused]] std::out_of_range& e) {
            auto response = ResponseFactory::not_found("404 Not Found", request.version());
            trigger_response_callbacks(response);
            co_return response;
        }

        RequestContext ctx(std::move(request));
        ctx.set_path_params(std::move(path_params));
        ctx.set_query_params(parse_query_params(query));

        if (middlewares_.empty()) {
            auto response = co_await handler(std::move(ctx));
            trigger_response_callbacks(response);
            co_return response;
        }
        Response response;
        // Note: Middleware integration would need adjustment for ContextHandler
        auto response_result = co_await handler(std::move(ctx));
        trigger_response_callbacks(response_result);
        co_return response_result;
    }

    void Server::merge_controller_routes(HttpController* controller) {
        controller->transfer_routes_to(registry_);
    }

    std::unordered_map<std::string, std::string> Server::parse_query_params(const std::string& query) {
        std::unordered_map<std::string, std::string> params;
        std::istringstream iss(query);
        std::string pair;

        while (std::getline(iss, pair, '&')) {
            if (const auto eq_pos = pair.find('=');
                eq_pos != std::string::npos) {
                auto key    = pair.substr(0, eq_pos);
                auto value  = pair.substr(eq_pos + 1);
                params[key] = std::move(value);
            }
        }

        return params;
    }

    // Non-blocking callback triggers
    void Server::trigger_start_callbacks() const {
        // Execute sync callbacks immediately
        for (const auto& callback : start_callbacks_) {
            try {
                callback();
            }
            catch (const std::exception& e) {
                trigger_error_callbacks(e);
            }
        }

        // Spawn async callbacks
        for (const auto& callback : async_start_callbacks_) {
            asio::co_spawn(
                ioc_,
                [this, callback]() -> AsyncVoid {
                    try {
                        co_await callback();
                    }
                    catch (const std::exception& e) {
                        trigger_error_callbacks(e);
                    }
                },
                asio::detached);
        }
    }

    void Server::trigger_stop_callbacks() const {
        for (const auto& callback : stop_callbacks_) {
            try {
                callback();
            }
            catch (const std::exception& e) {
                COMPONENT_LOG_ERR() << "Error in stop callback";
                trigger_error_callbacks(e);
            }
        }

        // Spawn async callbacks
        for (const auto& callback : async_stop_callbacks_) {
            asio::co_spawn(
                ioc_,
                [this, callback]() -> AsyncVoid {
                    try {
                        co_await callback();
                    }
                    catch (const std::exception& e) {
                        COMPONENT_LOG_ERR() << "Error in async stop callback";
                        trigger_error_callbacks(e);
                    }
                },
                asio::detached);
        }
    }

    void Server::trigger_request_callbacks(const Request& req) const {
        // Execute sync callbacks immediately
        for (const auto& callback : request_callbacks_) {
            try {
                callback(req);
            }
            catch (const std::exception& e) {
                COMPONENT_LOG_ERR() << "Error in request callback";
                trigger_error_callbacks(e);
            }
        }

        // Spawn async callbacks
        for (const auto& callback : async_request_callbacks_) {
            asio::co_spawn(
                ioc_,
                [this, callback, req]() -> AsyncVoid {
                    try {
                        co_await callback(req);
                    }
                    catch (const std::exception& e) {
                        COMPONENT_LOG_ERR() << "Error in async request callback";
                        trigger_error_callbacks(e);
                    }
                },
                asio::detached);
        }
    }

    void Server::trigger_response_callbacks(const Response& res) const {
        // Execute sync callbacks immediately
        for (const auto& callback : response_callbacks_) {
            try {
                callback(res);
            }
            catch (const std::exception& e) {
                COMPONENT_LOG_ERR() << "Error in response callback";
                trigger_error_callbacks(e);
            }
        }

        // Spawn async callbacks
        for (const auto& callback : async_response_callbacks_) {
            asio::co_spawn(
                ioc_,
                [this, callback, res]() -> AsyncVoid {
                    try {
                        co_await callback(res);
                    }
                    catch (const std::exception& e) {
                        COMPONENT_LOG_ERR() << "Error in async response callback";
                        trigger_error_callbacks(e);
                    }
                },
                asio::detached);
        }
    }

    void Server::trigger_error_callbacks(const std::exception& e) const {
        // Execute sync error callbacks immediately
        for (const auto& callback : error_callbacks_) {
            try {
                callback(e);
            }
            catch (...) {
                // If error callback itself fails, just log to stderr
                // Don't trigger more error callbacks to avoid infinite recursion
                COMPONENT_LOG_ERR() << "Error in error callback";
            }
        }

        // Spawn async error callbacks
        for (const auto& callback : async_error_callbacks_) {
            asio::co_spawn(
                ioc_,
                [callback, &e]() -> AsyncVoid {
                    try {
                        co_await callback(e);
                    }
                    catch (...) {
                        // Same logic - just log, don't recurse
                        COMPONENT_LOG_ERR() << "Error in async error callback";
                    }
                },
                asio::detached);
        }
    }
} // namespace demiplane::http
