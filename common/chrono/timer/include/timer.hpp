#pragma once
#include <chrono>
#include <concepts>
#include <functional>
#include <future>
#include <thread>
#include <utility>

#include <thread_pool.hpp>

#include "../cancellation_token.hpp"

namespace demiplane::chrono {
    class Timer : gears::NonCopyable {
        public:
        explicit Timer(const multithread::ThreadPoolConfig& config) {
            pool_ = std::make_shared<multithread::ThreadPool>(config);
        }

        explicit Timer(std::shared_ptr<multithread::ThreadPool> pool)
            : pool_(std::move(pool)) {
        }

        using clock = std::chrono::steady_clock;

        template <typename... Args, typename Callable>
            requires std::invocable<Callable, Args...>
        auto execute_polite_vanish(std::chrono::milliseconds timeout, Callable&& fn, Args&&... args);

        template <typename... Args, typename Callable>
            requires std::invocable<Callable, Args...>
        auto execute_violent_kill(std::chrono::milliseconds timeout,
                                  const std::shared_ptr<CancellationToken>& token,
                                  Callable&& fn,
                                  Args&&... args);

        private:
        std::shared_ptr<multithread::ThreadPool> pool_;

        // helper - default spawns a jthread; replace with thread-pool later
        template <typename F>
        auto spawn(F&& f) {
            return std::jthread{std::forward<F>(f)};
        }

        // type-trait: does first arg look like a cancellation token?
    };
}  // namespace demiplane::chrono

#include "../source/timer.inl"
