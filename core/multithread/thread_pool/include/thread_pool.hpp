#pragma once
#include <atomic>
#include <condition_variable>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

#include "../enqueued_task.hpp"

namespace demiplane::multithread {


    /// Possible enhancements:
    /// Overload of threads causes skipping Low priority tasks: suggested Round Robin
    /// Thread Affinity(latest)
    /// Timeout for thread
    /// Observers over function that allows to cancel task
    /// Resize of min_max thread limits

    class ThreadPool {
    public:
        using TaskPriority = uint32_t;
        // Constructs the thread pool with specified minimum and maximum threads.
        // - min_threads: Minimum number of threads that will always remain active.
        // - max_threads: Maximum number of threads allowed.
        // Notes:
        // - If min_threads == max_threads, the pool size remains fixed.
        // - If min_threads is set to 0, threads are only created dynamically when tasks are added.

        explicit ThreadPool(std::size_t min_threads, std::size_t max_threads);

        ~ThreadPool();

        // Add a task to the thread pool
        template <class Func, class... Args>
        std::future<std::invoke_result_t<Func, Args...>> enqueue(
            Func&& f, TaskPriority task_priority = 1, Args&&... args);

        void shutdown();

    private:
        void create_worker();

        std::vector<std::jthread> workers_; // Worker threads
        std::priority_queue<EnqueuedTask> tasks_; // Task queue using max-heap priority behavior

        std::mutex queue_mutex_; // Protects task queue
        std::condition_variable condition_; // Notify workers
        std::atomic<bool> stop_; // Stop flag

        std::size_t min_threads_; // Minimum threads
        std::size_t max_threads_; // Maximum threads
        std::atomic<size_t> active_threads_; // Count of active threads
    };

} // namespace demiplane::multithread

template <class Func, class... Args>
std::future<std::invoke_result_t<Func, Args...>> demiplane::multithread::ThreadPool::enqueue(
    Func&& f, TaskPriority task_priority, Args&&... args) {
    using return_type = std::invoke_result_t<Func, Args...>;

    // Wrap the task in a lambda instead of std::bind
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
        tasks_.emplace(task_priority, [task]() { (*task)(); });

        if (active_threads_ < max_threads_ && workers_.size() < max_threads_ && tasks_.size() > active_threads_) {
            create_worker();
        }
    }
    condition_.notify_one();
    return res;
}
