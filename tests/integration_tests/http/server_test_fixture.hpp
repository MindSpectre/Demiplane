#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <demiplane/chrono>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <thread>
#include <vector>

#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <controller.hpp>
#include <gtest/gtest.h>
#include <http11_config.hpp>
#include <http11_driver.hpp>
#include <request_context.hpp>
#include <server.hpp>
#include <server_config.hpp>

#include "http_test_fixture.hpp"  // TcpClient (reused Beast client)

namespace http_it {

    /// GET /ping → 200 "pong"; GET /boom → handler throw (driver 500).
    class PingController final : public demiplane::http::HttpController {
    public:
        void configure_routes() override {
            Get("/ping", &PingController::ping);
            Get("/boom", &PingController::boom);
        }

    private:
        demiplane::http::AsyncResponse ping(demiplane::http::RequestContext ctx) {
            co_return ctx.ok("pong");
        }
        demiplane::http::AsyncResponse boom(demiplane::http::RequestContext) {
            throw std::runtime_error{"handler exploded"};
        }
    };

    /// Timed handlers with entry latches so tests can deterministically wait
    /// until a request is IN FLIGHT before triggering shutdown.
    class LatchController final : public demiplane::http::HttpController {
    public:
        std::atomic<bool> slow_entered{false};
        std::atomic<bool> hang_entered{false};

        void configure_routes() override {
            Get("/slow", &LatchController::slow);  // finishes inside any sane drain window
            Get("/hang", &LatchController::hang);  // outlives a 100ms drain deadline
        }

    private:
        demiplane::http::AsyncResponse slow(demiplane::http::RequestContext ctx) {
            slow_entered.store(true, std::memory_order_release);
            co_await demiplane::chrono::async_sleep_for(std::chrono::milliseconds{150});
            co_return ctx.ok("slow done");
        }
        demiplane::http::AsyncResponse hang(demiplane::http::RequestContext ctx) {
            hang_entered.store(true, std::memory_order_release);
            co_await demiplane::chrono::async_sleep_for(std::chrono::milliseconds{500});
            co_return ctx.ok("hang done");
        }
    };

    /// Owns an io_context + N worker threads + an injected-executor Server.
    /// start_server() runs the build phase + setup() and goes live;
    /// TearDown() runs the full §9.7 caller sequence: stop → wait_until_stopped
    /// → THEN tear the executor down.
    class ServerIntegrationFixture : public ::testing::Test {
    protected:
        boost::asio::io_context ioc_;
        // Keeps ioc_.run() from returning between thread start and setup()'s
        // accept loops (and across the post-shutdown assertions).
        std::optional<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> guard_{
            boost::asio::make_work_guard(ioc_)};
        std::optional<demiplane::http::Server> server_;
        std::vector<std::thread> workers_;
        bool torn_down_ = false;

        void start_server(const std::function<void(demiplane::http::Server&)>& configure,
                          demiplane::http::ServerConfig cfg = demiplane::http::ServerConfig::Builder{}.finalize(),
                          const std::size_t io_threads      = 1) {
            server_.emplace(cfg, ioc_.get_executor());
            configure(*server_);
            // Workers BEFORE setup(): with observers registered, setup()
            // blocks on the on_setup_complete barrier, which needs a driven
            // executor (the work guard keeps ioc_.run() alive meanwhile).
            workers_.reserve(io_threads);
            for (std::size_t i = 0; i < io_threads; ++i) {
                workers_.emplace_back([this] { ioc_.run(); });
            }
            server_->setup();  // throws surface in the test body
        }

        [[nodiscard]] std::uint16_t port() const {
            return server_->listeners().front()->bound_port();
        }

        /// §9.7 caller sequence. Idempotent — callable from a test body and
        /// again from TearDown.
        void shutdown_and_join() {
            if (torn_down_) {
                return;
            }
            torn_down_ = true;
            if (server_) {
                server_->stop();                // idempotent; no-op before setup() / after stopped
                server_->wait_until_stopped();  // safe in every state (immediate for build/stopped)
            }
            guard_.reset();
            ioc_.stop();
            for (auto& t : workers_) {
                if (t.joinable()) {
                    t.join();
                }
            }
        }

        void TearDown() override {
            shutdown_and_join();
        }
    };

}  // namespace http_it
