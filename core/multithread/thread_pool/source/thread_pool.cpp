#include "thread_pool.hpp"
demiplane::multithread::ThreadPool::ThreadPool(const std::size_t min_threads, const std::size_t max_threads)
    : stop_(false), min_threads_(min_threads), max_threads_(max_threads), active_threads_(0) {
    if (min_threads > max_threads || max_threads == 0) {
        throw std::invalid_argument("Invalid thread pool size: min_threads must be <= max_threads and max_threads > 0");
    }
    for (std::size_t i = 0; i < min_threads; ++i) {
        create_worker();
    }
}

void demiplane::multithread::ThreadPool::create_worker() {
    workers_.emplace_back([this] {
        while (true) {
            EnqueuedTask task;
            {
                std::unique_lock lock(queue_mutex_);
                condition_.wait_for(lock, std::chrono::seconds(30), [this] { return stop_ || !tasks_.empty(); });
                if (stop_ && tasks_.empty()) {
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

void demiplane::multithread::ThreadPool::shutdown() {
    stop_ = true;
    condition_.notify_all();
}

demiplane::multithread::ThreadPool::~ThreadPool() {
    shutdown();
}

