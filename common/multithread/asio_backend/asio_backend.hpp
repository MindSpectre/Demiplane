#pragma once

#include <cstddef>
#include <thread>
#include <utility>
#include <vector>

#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>

#include "thread_pinning.hpp"  // pin_current_thread_to_core

namespace demiplane::multithread {

    /// RAII io_context + N std::jthread runners. Uses manual threads rather than
    /// `boost::asio::thread_pool` because we need a pre-`io_.run()` hook for
    /// `pin_current_thread_to_core` in the pinned variant.
    ///
    /// Destruction order: work_guard reset → io.stop() → jthread dtors join. All posted
    /// tasks must finish (or be cancelled) before whatever they reference is destroyed;
    /// declare that state earlier in the owning scope so this dtor runs first.
    class AsioBackend : gears::Immutable {
    public:
        /// @param n_threads runner threads to spawn.
        /// @param pin       pin each runner to a core when true.
        /// @param pin_cores affinity-window size: runner `i` pins to `(i % pin_cores) + base_core`.
        ///                  Caller-controlled so an over-subscribed thread count
        ///                  (`n_threads > pin_cores`) wraps back onto the intended cores
        ///                  rather than targeting (and silently failing on) absent cores.
        /// @param base_core first core of the affinity window (default 0).
        AsioBackend(const std::size_t n_threads, const bool pin, const std::size_t pin_cores, const int base_core = 0)
            : guard_(io_.get_executor()) {
            threads_.reserve(n_threads);
            for (std::size_t i = 0; i < n_threads; ++i) {
                threads_.emplace_back([this, i, pin, pin_cores, base_core] {
                    if (pin && pin_cores > 0) {
                        pin_current_thread_to_core(static_cast<int>(i % pin_cores) + base_core);
                    }
                    io_.run();
                });
            }
        }

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

}  // namespace demiplane::multithread
