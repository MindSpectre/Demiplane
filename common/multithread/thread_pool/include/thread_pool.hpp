#pragma once
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

#include <gears_class_traits.hpp>
#include <iostream>
#include <list>

#include "../enqueued_task.hpp"
#include "sync_resource.hpp"

namespace demiplane::multithread {

    class ThreadPool : gears::Immutable {
    public:
        using TaskPriority = uint32_t;

        ThreadPool(std::size_t               min_threads,
                   std::size_t               max_threads,
                   std::chrono::milliseconds idle_timeout = std::chrono::seconds(30));
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
            return min_threads_;
        }
        [[nodiscard]] std::size_t max_threads() const {
            return max_threads_;
        }
        [[nodiscard]] const std::atomic<size_t>& active_threads() const {
            return active_threads_;
        }
        [[nodiscard]] std::chrono::milliseconds idle_timeout() const {
            return idle_timeout_;
        }

    private:
        struct safe_thread {
            std::atomic<bool>         valid{true};
            std::jthread thread;
        };
        void create_worker();

        SyncResource<std::list<safe_thread>>           workers_;
        SyncResource<std::priority_queue<EnqueuedTask>> tasks_;

        std::mutex              queue_mutex_;
        std::condition_variable condition_;
        std::atomic<bool>       stop_;

        std::size_t               min_threads_;
        std::size_t               max_threads_;
        std::atomic<size_t>       active_threads_;
        std::chrono::milliseconds idle_timeout_;
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
        std::unique_lock lock(queue_mutex_);
        if (stop_) {
            throw std::runtime_error("ThreadPool is stopped");
        }
        tasks_.write()->emplace([task] { (*task)(); }, task_priority);
        workers_.with_lock([&](auto& list_) {
            std::erase_if(list_, [](const safe_thread& t) { return !t.valid; });
        });


// Create worker if needed and we haven't reached max threads
        if (workers_.read()->size() < max_threads_ && tasks_.read()->size() > active_threads_) {
            create_worker();
        }
    }
    condition_.notify_one();
    return res;
}
