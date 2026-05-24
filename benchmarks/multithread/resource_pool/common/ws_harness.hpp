#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <demiplane/multithread>  // demiplane::multithread::pin_current_thread_to_core
#include <memory>
#include <thread>
#include <vector>

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/as_tuple.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/local/connect_pair.hpp>
#include <boost/asio/local/stream_protocol.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/websocket.hpp>

namespace bench::pool {

    /// In-process WebSocket loopback used as the benchmark's "work" instrument.
    ///
    /// Owns N socketpairs (UNIX domain). For each, the *client* end is a beast
    /// websocket::stream bound to a caller-supplied worker executor (the worker writes a
    /// tiny frame per record); the *server* end is drained on one dedicated sink thread so
    /// writes complete at ~write-syscall cost. Handshakes finish behind a barrier before any
    /// measured work starts.
    class WsHarness {
    public:
        using LocalSocket = boost::asio::local::stream_protocol::socket;
        using WsStream    = boost::beast::websocket::stream<LocalSocket>;

        /// `client_execs[w]` is the executor worker w runs on; `sink_core` is the core to
        /// pin the single drain thread to (share with the mostly-idle producer).
        WsHarness(const std::vector<boost::asio::any_io_executor>& client_execs, const int sink_core)
            : sink_guard_{boost::asio::make_work_guard(sink_ioc_)},
              sinks_live_{client_execs.size()} {
            const std::size_t n = client_execs.size();
            clients_.reserve(n);
            servers_.reserve(n);
            for (std::size_t w = 0; w < n; ++w) {
                LocalSocket cs{client_execs[w]};
                LocalSocket ss{sink_ioc_.get_executor()};
                boost::asio::local::connect_pair(cs, ss);  // socketpair() + assign fds
                clients_.push_back(std::make_unique<WsStream>(std::move(cs)));
                servers_.push_back(std::make_unique<WsStream>(std::move(ss)));
                disable_timeouts(*clients_[w]);
                disable_timeouts(*servers_[w]);
                clients_[w]->binary(true);  // tiny binary frame as the work payload
            }
            sink_thread_ = std::jthread{[this, sink_core] {
                demiplane::multithread::pin_current_thread_to_core(sink_core);
                sink_ioc_.run();
            }};
        }

        WsHarness(const WsHarness&)            = delete;
        WsHarness& operator=(const WsHarness&) = delete;

        ~WsHarness() {
            // Defensive: if await_sinks_and_stop() wasn't called, still stop the ioc so the
            // jthread joins (server streams must already be torn down by then).
            sink_guard_.reset();
            sink_ioc_.stop();
        }

        [[nodiscard]] WsStream& client(const std::size_t w) noexcept {
            return *clients_[w];
        }
        [[nodiscard]] std::size_t size() const noexcept {
            return clients_.size();
        }

        /// Spawn client handshakes (on each client executor) + server accept-then-drain (on the
        /// sink ioc); block until every handshake has completed. Call AFTER the worker backend
        /// threads are running and BEFORE the producer starts.
        void start_and_await_handshakes() {
            using namespace std::chrono_literals;
            const std::size_t n = clients_.size();
            for (std::size_t w = 0; w < n; ++w) {
                boost::asio::co_spawn(
                    clients_[w]->get_executor(),
                    [this, w]() -> boost::asio::awaitable<void> {
                        co_await clients_[w]->async_handshake("bench", "/", boost::asio::use_awaitable);
                        handshakes_.fetch_add(1, std::memory_order_acq_rel);
                    },
                    boost::asio::detached);
                boost::asio::co_spawn(
                    sink_ioc_.get_executor(),
                    [this, w]() -> boost::asio::awaitable<void> {
                        co_await servers_[w]->async_accept(boost::asio::use_awaitable);
                        boost::beast::flat_buffer buf;
                        for (;;) {
                            auto [ec, n2] = co_await servers_[w]->async_read(
                                buf, boost::asio::as_tuple(boost::asio::use_awaitable));
                            if (ec) {
                                break;  // client closed => EOF
                            }
                            buf.consume(buf.size());
                        }
                        sinks_live_.fetch_sub(1, std::memory_order_acq_rel);  // SINGLE exit
                    },
                    boost::asio::detached);
            }
            while (handshakes_.load(std::memory_order_acquire) < n) {
                std::this_thread::sleep_for(1ms);
            }
        }

        /// Close every client stream (each close posted to its own executor, so it runs on the
        /// worker's thread), then wait for the drain coroutines to observe EOF and exit, then
        /// stop the sink ioc and join. Call once, after all workers have finished.
        void shutdown() {
            using namespace std::chrono_literals;
            const std::size_t n = clients_.size();
            std::atomic<std::size_t> closed{0};
            for (std::size_t w = 0; w < n; ++w) {
                boost::asio::post(clients_[w]->get_executor(), [this, w, &closed] {
                    boost::system::error_code ec;
                    clients_[w]->next_layer().close(ec);  // NOLINT(bugprone-unused-return-value): ec holds errors
                    closed.fetch_add(1, std::memory_order_acq_rel);
                });
            }
            while (closed.load(std::memory_order_acquire) < n) {
                std::this_thread::sleep_for(1ms);
            }
            while (sinks_live_.load(std::memory_order_acquire) != 0) {
                std::this_thread::sleep_for(1ms);
            }
            sink_guard_.reset();
            sink_ioc_.stop();
            if (sink_thread_.joinable()) {
                sink_thread_.join();
            }
        }

    private:
        static void disable_timeouts(WsStream& ws) {
            boost::beast::websocket::stream_base::timeout t{};
            t.handshake_timeout = boost::beast::websocket::stream_base::none();
            t.idle_timeout      = boost::beast::websocket::stream_base::none();
            t.keep_alive_pings  = false;
            ws.set_option(t);
        }

        boost::asio::io_context sink_ioc_;
        boost::asio::executor_work_guard<boost::asio::io_context::executor_type> sink_guard_;
        std::vector<std::unique_ptr<WsStream>> clients_;
        std::vector<std::unique_ptr<WsStream>> servers_;
        std::atomic<std::size_t> handshakes_{0};
        std::atomic<std::size_t> sinks_live_;
        std::jthread sink_thread_;
    };

}  // namespace bench::pool
