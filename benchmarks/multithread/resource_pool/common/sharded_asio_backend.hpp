#pragma once

#include <cstddef>
#include <demiplane/multithread>
#include <memory>
#include <thread>
#include <vector>

#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <sys/prctl.h>

#include "bench_scenarios.hpp"  // CORE_COUNT

namespace bench::pool {

    /// N single-threaded io_contexts (one per core). Each context's scheduler lock is
    /// touched only by its own runner thread plus cross-shard wake posts, so the heavy
    /// shared-scheduler-lock contention of one io_context run by N threads disappears.
    /// Single-threaded contexts also need no per-coroutine strand.
    class ShardedAsioBackend {
    public:
        ShardedAsioBackend(const std::size_t n, const bool pin)
            : n_{n} {
            contexts_.reserve(n);
            guards_.reserve(n);
            for (std::size_t i = 0; i < n; ++i) {
                contexts_.push_back(std::make_unique<boost::asio::io_context>());
                guards_.push_back(boost::asio::make_work_guard(*contexts_[i]));
            }
            threads_.reserve(n);
            for (std::size_t i = 0; i < n; ++i) {
                threads_.emplace_back([this, i, pin] {
                    if (pin) {
                        demiplane::multithread::pin_current_thread_to_core(static_cast<int>(i % CORE_COUNT));
                    }
                    // 50us default timer slack -> ~1ns; sharpens timerfd/epoll waits on this io
                    // thread (the pool's acquire_for timeout + the dispatcher poll).
                    ::prctl(PR_SET_TIMERSLACK, 1UL, 0, 0, 0);
                    contexts_[i]->run();
                });
            }
        }

        ShardedAsioBackend(const ShardedAsioBackend&)            = delete;
        ShardedAsioBackend& operator=(const ShardedAsioBackend&) = delete;

        ~ShardedAsioBackend() noexcept {
            for (auto& g : guards_) {
                g.reset();
            }
            for (auto& c : contexts_) {
                c->stop();
            }
        }

        [[nodiscard]] boost::asio::io_context::executor_type executor(const std::size_t i) {
            return contexts_[i % n_]->get_executor();
        }

        [[nodiscard]] std::size_t size() const noexcept {
            return n_;
        }

    private:
        std::size_t n_;
        std::vector<std::unique_ptr<boost::asio::io_context>> contexts_;
        std::vector<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> guards_;
        std::vector<std::jthread> threads_;
    };

}  // namespace bench::pool
