#pragma once

#include <cstddef>
#include <memory>
#include <thread>
#include <vector>

#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>

#if defined(__linux__)
    #include <sys/prctl.h>
#endif

#include "thread_pinning.hpp"  // pin_current_thread_to_core

namespace demiplane::multithread {

    /// N single-threaded io_contexts (one per shard). Each context's scheduler lock is
    /// touched only by its own runner thread plus cross-shard wake posts, so the heavy
    /// shared-scheduler-lock contention of one io_context run by N threads disappears.
    /// Single-threaded contexts also need no per-coroutine strand.
    class ShardedAsioBackend : gears::NonCopyable {
    public:
        /// @param n         number of shards (single-threaded io_contexts).
        /// @param pin       pin each shard's runner to a core when true.
        /// @param pin_cores affinity-window size: shard `i` pins to `(i % pin_cores) + base_core`.
        /// @param base_core first core of the affinity window (default 0).
        ShardedAsioBackend(const std::size_t n, const bool pin, const std::size_t pin_cores, const int base_core = 0)
            : n_{n} {
            contexts_.reserve(n);
            guards_.reserve(n);
            for (std::size_t i = 0; i < n; ++i) {
                contexts_.push_back(std::make_unique<boost::asio::io_context>());
                guards_.push_back(boost::asio::make_work_guard(*contexts_[i]));
            }
            threads_.reserve(n);
            for (std::size_t i = 0; i < n; ++i) {
                threads_.emplace_back([this, i, pin, pin_cores, base_core] {
                    if (pin && pin_cores > 0) {
                        pin_current_thread_to_core(static_cast<int>(i % pin_cores) + base_core);
                    }
#if defined(__linux__)
                    // 50us default timer slack -> ~1ns; sharpens timerfd/epoll waits on this io
                    // thread (e.g. an acquire_for timeout + a dispatcher poll).
                    ::prctl(PR_SET_TIMERSLACK, 1UL, 0, 0, 0);
#endif
                    contexts_[i]->run();
                });
            }
        }

        ~ShardedAsioBackend() noexcept {
            for (auto& g : guards_) {
                g.reset();
            }
            for (const auto& c : contexts_) {
                c->stop();
            }
        }

        [[nodiscard]] boost::asio::io_context::executor_type executor(const std::size_t i) const {
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

}  // namespace demiplane::multithread
