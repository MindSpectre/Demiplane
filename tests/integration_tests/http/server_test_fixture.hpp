#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <demiplane/chrono>
#include <deque>
#include <functional>
#include <optional>
#include <stdexcept>
#include <thread>
#include <vector>

#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/this_coro.hpp>
#include <controller.hpp>
#include <gtest/gtest.h>
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
        static demiplane::http::AsyncResponse ping(demiplane::http::RequestContext ctx) {
            co_return ctx.ok("pong");
        }
        static demiplane::http::AsyncResponse boom(demiplane::http::RequestContext) {
            throw std::runtime_error{"handler exploded"};
        }
    };

    /// Timed handlers with entry COUNTERS so tests can deterministically wait
    /// until every request they issued is IN FLIGHT before triggering shutdown.
    ///
    /// Counters, not flags: a flag only proves that ONE handler was entered,
    /// which says nothing about the other connections a multi-client test
    /// opened. Those may still be sitting in the kernel accept backlog when
    /// stop() closes the acceptor — the kernel then RSTs them and the client
    /// read fails with ECONNRESET. Wait for the FULL count instead.
    class LatchController final : public demiplane::http::HttpController {
    public:
        std::atomic<int> slow_entries{0};
        std::atomic<int> hang_entries{0};

        void configure_routes() override {
            Get("/slow", &LatchController::slow);  // finishes inside any sane drain window
            Get("/hang", &LatchController::hang);  // outlives a 100ms drain deadline
        }

    private:
        demiplane::http::AsyncResponse slow(demiplane::http::RequestContext ctx) {
            slow_entries.fetch_add(1, std::memory_order_release);
            boost::asio::steady_timer t{co_await boost::asio::this_coro::executor};
            t.expires_after(std::chrono::milliseconds{150});
            co_await t.async_wait(demiplane::http::use_strand_awaitable);
            co_return ctx.ok("slow done");
        }
        demiplane::http::AsyncResponse hang(demiplane::http::RequestContext ctx) {
            hang_entries.fetch_add(1, std::memory_order_release);
            boost::asio::steady_timer t{co_await boost::asio::this_coro::executor};
            t.expires_after(std::chrono::milliseconds{500});
            co_await t.async_wait(demiplane::http::use_strand_awaitable);
            co_return ctx.ok("hang done");
        }
    };

    /// Owns N io_contexts (one worker thread each — run_standalone's topology,
    /// README Finding 13) + an injected-executor Server. start_server() runs
    /// the build phase + setup() and goes live; TearDown() runs the full §9.7
    /// caller sequence: stop → wait_until_stopped → THEN tear the executors
    /// down. `io_threads` is the number of io_contexts.
    class ServerIntegrationFixture : public ::testing::Test {
    protected:
        // deque: io_context is immovable; must outlive server_.
        std::deque<boost::asio::io_context> contexts_;
        // Keep each ctx.run() from returning between thread start and setup()'s
        // accept loops (and across the post-shutdown assertions).
        std::vector<boost::asio::executor_work_guard<demiplane::http::Executor>> guards_;
        std::optional<demiplane::http::Server> server_;
        std::vector<std::thread> workers_;
        bool torn_down_ = false;

        void start_server(const std::function<void(demiplane::http::Server&)>& configure,
                          demiplane::http::ServerConfig cfg = demiplane::http::ServerConfig::Builder{}.finalize(),
                          const std::size_t io_threads      = 1) {
            std::vector<demiplane::http::Executor> execs;
            execs.reserve(io_threads);
            for (std::size_t i = 0; i < io_threads; ++i) {
                auto& ctx = contexts_.emplace_back();
                guards_.emplace_back(boost::asio::make_work_guard(ctx));
                execs.push_back(ctx.get_executor());
            }
            server_.emplace(std::move(cfg), execs);
            configure(*server_);
            // Workers BEFORE setup(): with observers registered, setup()
            // blocks on the on_setup_complete barrier, which needs a driven
            // control executor (the work guards keep the runs alive meanwhile).
            workers_.reserve(io_threads);
            for (auto& ctx : contexts_) {
                workers_.emplace_back([&ctx] { ctx.run(); });
            }
            server_->setup();  // throws surface in the test body
        }

        /// The control context (execs.front()) — post work here to prove the
        /// caller's executor survives stop() (§9.7 assertions).
        [[nodiscard]] boost::asio::io_context& control_context() {
            return contexts_.front();
        }

        /// For tests that construct a Server directly (no start_server): one
        /// context, no worker thread — fine for build-phase-only assertions
        /// (setup() throws before anything needs a driven executor).
        [[nodiscard]] demiplane::http::Executor bootstrap_executor() {
            auto& ctx = contexts_.emplace_back();
            guards_.emplace_back(boost::asio::make_work_guard(ctx));
            return ctx.get_executor();
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
            for (auto& g : guards_) {
                g.reset();
            }
            for (auto& ctx : contexts_) {
                ctx.stop();
            }
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
