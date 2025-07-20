#pragma once
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <iostream>
#include <list>

#include <gears_class_traits.hpp>
#include <thread_safe_resource.hpp>

#include "../enqueued_task.hpp"
#include "thread_pool_config.hpp"

namespace demiplane::multithread {
    class ThreadPool : gears::Immutable {
    public:
        using TaskPriority = uint32_t;

        explicit ThreadPool(const ThreadPoolConfig& config) {
            if (!config.ok()) {
                throw std::invalid_argument("Invalid config");
            }
            config_ = config;
            for (std::size_t i = 0; i < min_threads(); ++i) {
                create_worker();
            }
            if (config_.enable_cleanup_thread) {
                start_cleanup_thread();
            }
        }

        ~ThreadPool();

        template <class Func, class... Args>
        std::future<std::invoke_result_t<Func, Args...>>
        enqueue(Func&& f, TaskPriority task_priority = 1, Args&&... args);

        void shutdown();


        [[nodiscard]] bool is_running() const {
            return !stop_;
        }

        [[nodiscard]] std::size_t size() const {
            return workers_.read()->size();
        }

        [[nodiscard]] std::size_t min_threads() const {
            return config_.min_threads;
        }

        [[nodiscard]] std::size_t max_threads() const {
            return config_.max_threads;
        }

        [[nodiscard]] size_t active_threads() const {
            return active_threads_.load();
        }

        [[nodiscard]] const ThreadPoolConfig& config() const {
            return config_;
        }

        [[nodiscard]] bool is_full() const {
            return size() >= max_threads();
        }

        [[nodiscard]] std::chrono::milliseconds idle_timeout() const {
            return config_.idle_timeout;
        }

        [[nodiscard]] std::chrono::milliseconds cleanup_interval() const {
            return config_.cleanup_interval;
        }

    private:
        struct safe_thread {
            std::atomic<bool> valid{true};
            std::jthread      thread;
        };

        void                                                  create_worker();
        void                                                  start_cleanup_thread();
        void                                                  cleanup_invalid_workers();
        ThreadSafeResource<std::list<safe_thread>>            workers_;
        ThreadSafeResource<std::priority_queue<EnqueuedTask>> tasks_;

        std::mutex              task_queue_mutex_;
        std::condition_variable task_condition_;

        std::jthread            cleanup_thread_;
        std::condition_variable cleanup_condition_;
        std::mutex              cleanup_mutex_;

        std::atomic<bool> stop_{false};

        ThreadPoolConfig    config_{};
        std::atomic<size_t> active_threads_{0};
    };
} // namespace demiplane::multithread

template <class Func, class... Args>
std::future<std::invoke_result_t<Func, Args...>>
demiplane::multithread::ThreadPool::enqueue(Func&& f, TaskPriority task_priority, Args&&... args) {
    using return_type = std::invoke_result_t<Func, Args...>;

    auto task = std::make_shared<std::packaged_task<return_type()>>(
        [func = std::forward<Func>(f), ... args = std::forward<Args>(args)]() mutable {
            return std::invoke(std::move(func), std::move(args)...);
        });

    std::future<return_type> res = task->get_future();
    {
        std::unique_lock lock(task_queue_mutex_);
        if (stop_) {
            throw std::runtime_error("ThreadPool is stopped");
        }
        tasks_.write()->emplace([task] {
            (*task)();
        }, task_priority);

        cleanup_invalid_workers();
        // Create worker if needed and we haven't reached max threads
        if (!is_full() && active_threads() <= size()) {
            create_worker();
        }
    }
    task_condition_.notify_one();
    return res;
}
