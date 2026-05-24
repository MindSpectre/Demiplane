#pragma once

#include <cstddef>
#include <demiplane/multithread>
#include <utility>
#include <vector>

#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>

#include "bench_scenarios.hpp"
namespace bench::pool {

    /// RAII io_context + N std::jthread runners. Uses manual threads rather
    /// than `boost::asio::thread_pool` because we need pre-`io_.run()` hooks
    /// for `pthread_setaffinity_np` in the pinned variant.
    ///
    /// Destruction order: work_guard reset → io.stop() → jthread dtors join.
    /// All posted tasks must finish (or be cancelled) before the pool they
    /// reference is destroyed; the runner that owns this backend declares the
    /// pool earlier in its scope, so this dtor runs first.
    class AsioBackend {
    public:
        AsioBackend(const std::size_t n_workers, const bool pin)
            : guard_(io_.get_executor()) {
            threads_.reserve(n_workers);
            for (std::size_t i = 0; i < n_workers; ++i) {
                threads_.emplace_back([this, i, pin] {
                    if (pin) {
                        demiplane::multithread::pin_current_thread_to_core(static_cast<int>(i % CORE_COUNT));
                    }
                    io_.run();
                });
            }
        }

        AsioBackend(const AsioBackend&)            = delete;
        AsioBackend& operator=(const AsioBackend&) = delete;
        AsioBackend(AsioBackend&&)                 = delete;
        AsioBackend& operator=(AsioBackend&&)      = delete;

        ~AsioBackend() noexcept {
            guard_.reset();
            io_.stop();
            // jthread dtors join the workers
        }

        template <typename F>
        void post(F&& f) {
            boost::asio::post(io_, std::forward<F>(f));
        }

        /// Executor for co_spawn-ing coroutines onto the backend's io_context.
        [[nodiscard]] boost::asio::io_context::executor_type get_executor() noexcept {
            return io_.get_executor();
        }

    private:
        boost::asio::io_context io_;
        boost::asio::executor_work_guard<boost::asio::io_context::executor_type> guard_;
        std::vector<std::jthread> threads_;
    };

}  // namespace bench::pool
