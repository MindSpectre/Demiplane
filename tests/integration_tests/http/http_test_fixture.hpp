#pragma once

#include <chrono>
#include <cstdint>
#include <future>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/use_future.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/http.hpp>
#include <controller.hpp>
#include <group.hpp>
#include <gtest/gtest.h>
#include <http11_config.hpp>
#include <http11_driver.hpp>
#include <listener_base.hpp>
#include <route_registry.hpp>
#include <router.hpp>
#include <tcp_listener.hpp>

namespace http_it {

    namespace beast = boost::beast;
    namespace asio  = boost::asio;
    namespace bhttp = boost::beast::http;

    using ParsedResponse = bhttp::response<bhttp::string_body>;

    /// Owns an io_context + one worker thread + a route registry + a listener
    /// bound to 127.0.0.1:0. Subclasses register controllers in SetUp() then call
    /// start(make_listener()). TearDown() runs the §9.7 shutdown sequence.
    class HttpIntegrationFixture : public ::testing::Test {
    protected:
        asio::io_context ioc_;
        demiplane::http::RouteRegistry registry_;
        std::vector<std::shared_ptr<demiplane::http::HttpController>> controllers_;
        std::optional<demiplane::http::Router> router_;
        std::unique_ptr<demiplane::http::ListenerBase> listener_;
        asio::cancellation_signal stop_signal_;
        std::future<void> run_future_;  // run()'s completion — acceptor provably closed after get()
        std::thread worker_;
        std::uint16_t port_{0};
        bool shut_down_ = false;

        void add_controller(std::shared_ptr<demiplane::http::HttpController> ctrl) {
            demiplane::http::GroupBinding{registry_, controllers_, ""}.add_controller(std::move(ctrl));
        }

        /// Freeze routes, bind the listener, go live on the worker thread.
        void start(std::unique_ptr<demiplane::http::ListenerBase> listener) {
            ASSERT_TRUE(registry_.freeze().empty()) << "route conflicts in test setup";
            router_.emplace(registry_);
            // Bind BEFORE adopting the listener or starting the worker: a bind()
            // throw leaves listener_ null and worker_ unstarted, so TearDown's
            // shutdown path no-ops instead of waiting on a drain nobody runs.
            listener->bind();
            port_ = listener->bound_port();
            ASSERT_GT(port_, 0);
            listener_   = std::move(listener);
            run_future_ = asio::co_spawn(
                ioc_, listener_->run(*router_), asio::bind_cancellation_slot(stop_signal_.slot(), asio::use_future));
            worker_ = std::thread{[this] { ioc_.run(); }};
        }

        /// Build the default plain-TCP / Http11 listener.
        std::unique_ptr<demiplane::http::ListenerBase> make_tcp_listener() {
            return std::make_unique<demiplane::http::TcpListener<demiplane::http::Http11Driver>>(
                ioc_.get_executor(), "127.0.0.1", 0, demiplane::http::Http11Driver{demiplane::http::Http11Config{}});
        }

        /// Emit the stop signal (stops accepting) and drain in-flight connections
        /// up to `drain`. Idempotent — safe to call from a test then again in
        /// TearDown. Mirrors Server::graceful_shutdown's drain phase (PR 5).
        void graceful_shutdown(std::chrono::milliseconds drain = std::chrono::seconds{2}) {
            if (shut_down_ || !listener_) {
                return;
            }
            shut_down_ = true;
            auto fut   = asio::co_spawn(
                ioc_,
                [this, drain]() -> asio::awaitable<void> {
                    stop_signal_.emit(asio::cancellation_type::terminal);
                    co_await listener_->drain_until(std::chrono::steady_clock::now() + drain);
                },
                asio::use_future);
            fut.get();  // blocks the (non-io) test thread until drain completes (§9.7)
            // The stop emit only CANCELS the pending accept; acceptor_.close()
            // runs in run()'s final turn, which is queued BEHIND the drain
            // future's satisfaction. Await run()'s completion so "no new
            // connections" is provably true when this returns — otherwise a
            // fresh connect() races the close and lands in the kernel backlog
            // (deterministic failure on a single-core machine).
            if (run_future_.valid()) {
                try {
                    run_future_.get();
                } catch (const std::exception& e) {
                    ADD_FAILURE() << "listener run() escaped with: " << e.what();
                }
            }
        }

        void TearDown() override {
            graceful_shutdown();
            // Unwind barrier: drain only DISPATCHES force-cancels; the cancelled
            // serve() coroutines unwind in later executor turns. Stopping the
            // io_context with frames still frozen would leave ~io_context (the
            // last-destroyed member) to destroy them AFTER listener_/tracker are
            // gone — Handle::release() on freed memory.
            if (listener_) {
                const auto cap = std::chrono::steady_clock::now() + std::chrono::seconds{1};
                while (listener_->in_flight() > 0 && std::chrono::steady_clock::now() < cap) {
                    std::this_thread::sleep_for(std::chrono::milliseconds{1});
                }
                if (listener_->in_flight() > 0) {
                    ADD_FAILURE() << "in-flight connections failed to unwind within 1s of drain";
                }
            }
            ioc_.stop();
            if (worker_.joinable()) {
                worker_.join();
            }
        }
    };

    /// Synchronous Beast client over one TCP socket — reusable for keep-alive.
    class TcpClient {
    public:
        explicit TcpClient(std::uint16_t port)
            : socket_{ioc_} {
            socket_.connect({asio::ip::make_address("127.0.0.1"), port});
        }

        ParsedResponse send(bhttp::verb verb,
                            std::string_view target,
                            std::string body              = {},
                            std::string_view content_type = "text/plain",
                            bool keep_alive               = false) {
            bhttp::request<bhttp::string_body> req{verb, target, 11};
            req.set(bhttp::field::host, "127.0.0.1");
            req.keep_alive(keep_alive);
            if (!body.empty()) {
                req.set(bhttp::field::content_type, std::string{content_type});
                req.body() = std::move(body);
            }
            req.prepare_payload();
            bhttp::write(socket_, req);

            ParsedResponse res;
            bhttp::read(socket_, buffer_, res);
            return res;
        }

        /// Send a request WITHOUT reading the response — for tests that expect
        /// the server to kill the connection mid-handler (pair with
        /// read_after_close()).
        void write_request(const bhttp::verb verb, const std::string_view target, const bool keep_alive = false) {
            bhttp::request<bhttp::string_body> req{verb, target, 11};
            req.set(bhttp::field::host, "127.0.0.1");
            req.keep_alive(keep_alive);
            req.prepare_payload();
            bhttp::write(socket_, req);
        }

        /// Read one response for a previously write_request()-ed request —
        /// lets a test trigger server-side events (e.g. graceful shutdown)
        /// BETWEEN sending and receiving.
        ParsedResponse read_response() {
            ParsedResponse res;
            bhttp::read(socket_, buffer_, res);
            return res;
        }

        /// Attempt to read a response; returns the resulting error_code. After the
        /// server force-cancels + half-closes, this returns a non-empty ec
        /// (end_of_stream / connection_reset).
        beast::error_code read_after_close() {
            ParsedResponse res;
            beast::error_code ec;
            bhttp::read(socket_, buffer_, res, ec);
            return ec;
        }

        /// HEAD request. The response has no body regardless of
        /// Content-Length, so the parser must be told to skip it.
        ParsedResponse send_head(const std::string_view target) {
            bhttp::request<bhttp::empty_body> req{bhttp::verb::head, target, 11};
            req.set(bhttp::field::host, "127.0.0.1");
            bhttp::write(socket_, req);

            bhttp::response_parser<bhttp::string_body> parser;
            parser.skip(true);
            bhttp::read(socket_, buffer_, parser);
            return parser.release();
        }

    private:
        asio::io_context ioc_;
        asio::ip::tcp::socket socket_;
        beast::flat_buffer buffer_;
    };

}  // namespace http_it
