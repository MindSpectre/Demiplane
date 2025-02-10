#pragma once
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>


namespace common::multithread {
    /// Possible enhancements:
    /// Overload of threads causes skipping Low priority tasks: suggested Round Robin
    /// Thread Affinity(latest)
    /// Timeout for thread
    /// Observers over function that allows to cancel task
    /// Resize of min_max thread limits
    class ThreadPool {
    public:
        // Constructs the thread pool with specified minimum and maximum threads.
        // - min_threads: Minimum number of threads that will always remain active.
        // - max_threads: Maximum number of threads allowed.
        // Notes:
        // - If min_threads == max_threads, the pool size remains fixed.
        // - If min_threads is set to 0, threads are only created dynamically when tasks are added.
        enum class TaskPriority { Low = 0, Medium = 1, High = 2, Extreme = 3 };

        explicit ThreadPool(std::size_t min_threads, std::size_t max_threads);

        ~ThreadPool();

        // Add a task to the thread pool
        template <class Func, class... Args>
        std::future<std::invoke_result_t<Func, Args...>> enqueue(
            Func&& f, TaskPriority task_priority = TaskPriority::Medium, Args&&... args);

        void shutdown();
        ThreadPool(const ThreadPool& other)                = delete;
        ThreadPool(ThreadPool&& other) noexcept            = delete;
        ThreadPool& operator=(const ThreadPool& other)     = delete;
        ThreadPool& operator=(ThreadPool&& other) noexcept = delete;

    private:
        void create_worker();

        class EnqueuedTask {
        public:
            void execute() {
                last_active_time_ = std::chrono::steady_clock::now();
                task();
            }
            void set_priority(const TaskPriority priority) {
                priority_ = priority;
            }
            [[nodiscard]] TaskPriority priority() const {
                return priority_;
            }

            [[nodiscard]] std::chrono::time_point<std::chrono::steady_clock> last_execute() const {
                return last_active_time_;
            }
            EnqueuedTask(const TaskPriority priority, std::function<void()> task)
                : priority_{priority}, task{std::move(task)} {
                last_active_time_ = std::chrono::steady_clock::now();
            }
            EnqueuedTask() {
                last_active_time_ = std::chrono::steady_clock::now();
            }

        private:
            TaskPriority priority_ = TaskPriority::Medium;
            std::function<void()> task;
            std::chrono::steady_clock::time_point last_active_time_;

            friend bool operator<(const EnqueuedTask& lhs, const EnqueuedTask& rhs) {
                return lhs.priority_ < rhs.priority_;
            }

            friend bool operator<=(const EnqueuedTask& lhs, const EnqueuedTask& rhs) {
                return !(lhs > rhs);
            }

            friend bool operator>(const EnqueuedTask& lhs, const EnqueuedTask& rhs) {
                return lhs.priority_ > rhs.priority_; // Reverse logic for greater comparison.
            }

            friend bool operator>=(const EnqueuedTask& lhs, const EnqueuedTask& rhs) {
                return !(lhs < rhs);
            }
        };

        std::vector<std::jthread> workers_; // Worker threads
        std::priority_queue<EnqueuedTask> tasks_; // Task queue using max-heap priority behavior

        std::mutex queue_mutex_; // Protects task queue
        std::condition_variable condition_; // Notify workers
        std::atomic<bool> stop; // Stop flag

        std::size_t min_threads_; // Minimum threads
        std::size_t max_threads_; // Maximum threads
        std::atomic<size_t> active_threads_; // Count of active threads
    };

    inline ThreadPool::ThreadPool(const std::size_t min_threads, const std::size_t max_threads)
        : stop(false), min_threads_(min_threads), max_threads_(max_threads), active_threads_(0) {
        if (min_threads > max_threads || max_threads == 0) {
            throw std::invalid_argument(
                "Invalid thread pool size: min_threads must be <= max_threads and max_threads > 0");
        }
        for (std::size_t i = 0; i < min_threads; ++i) {
            create_worker();
        }
    }

    inline void ThreadPool::create_worker() {
        workers_.emplace_back([this] {
            while (true) {
                EnqueuedTask task;
                {
                    std::unique_lock lock(queue_mutex_);
                    condition_.wait_for(lock, std::chrono::seconds(30), [this] { return stop || !tasks_.empty(); });
                    if (stop && tasks_.empty()) {
                        return;
                    }
                    if (!tasks_.empty()) {
                        task = tasks_.top();
                        tasks_.pop();
                    } else if (workers_.size() > min_threads_
                               && std::chrono::steady_clock::now() - task.last_execute() > std::chrono::seconds(30)) {
                        return; // Exit idle worker
                    }
                }
                ++active_threads_;
                task.execute();
                --active_threads_;
            }
        });
    }

    inline void ThreadPool::shutdown() {
        stop = true;
        condition_.notify_all();
    }

    inline ThreadPool::~ThreadPool() {
        shutdown();
    }

    template <class Func, class... Args>
    std::future<std::invoke_result_t<Func, Args...>> ThreadPool::enqueue(
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
            if (stop) {
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


} // namespace common::multithread
