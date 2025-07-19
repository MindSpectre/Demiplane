#include "thread_pool.hpp"
#include <stdexcept>

demiplane::multithread::ThreadPool::ThreadPool(const std::size_t         min_threads,
                                               const std::size_t         max_threads,
                                               std::chrono::milliseconds idle_timeout)
    : stop_(false),
      min_threads_(min_threads),
      max_threads_(max_threads),
      active_threads_(0),
      idle_timeout_(idle_timeout) {
    if (min_threads > max_threads || max_threads == 0) {
        throw std::invalid_argument("Invalid thread pool size: min_threads must be <= max_threads and max_threads > 0");
    }
    for (std::size_t i = 0; i < min_threads; ++i) {
        create_worker();
    }
}

void demiplane::multithread::ThreadPool::create_worker() {
    workers_->emplace_back();
    auto& worker = workers_->back();

    worker.thread = std::jthread{[this, &worker] {
        worker.valid       = true;
        auto last_activity = std::chrono::steady_clock::now();
        while (true) {
            EnqueuedTask task{nullptr, 0}; // Default invalid task
            bool         has_task = false;

            {
                std::unique_lock lock(queue_mutex_);
                condition_.wait_for(lock, idle_timeout_, [this] { return stop_ || !tasks_.read()->empty(); });

                // Check exit conditions first
                if (stop_ && tasks_.read()->empty()) {
                    break; // Shutdown requested and no more work
                }

                if (!tasks_.read()->empty()) {
                    // We have work to do
                    task = tasks_.read()->top();
                    tasks_.write()->pop();
                    has_task      = true;
                    last_activity = std::chrono::steady_clock::now();
                }
                else {
                    // No tasks available - check if we should exit due to idle timeout
                    if (workers_.read()->size() > min_threads_ &&
                        std::chrono::steady_clock::now() - last_activity > idle_timeout_) {
                        break; // Exit idle worker
                    }
                    // Otherwise, continue the loop (spurious wake-up or brief timeout)
                }
            } // Release lock here

            // Execute task outside the lock
            if (has_task) {
                ++active_threads_;
                task.execute();
                --active_threads_;
            }
        }
        worker.valid = false;
    }};
}

void demiplane::multithread::ThreadPool::shutdown() {
    {
        std::unique_lock lock(queue_mutex_);
        stop_ = true;
    }
    condition_.notify_all();
    workers_.write()->clear();
}

demiplane::multithread::ThreadPool::~ThreadPool() {
    shutdown();
}
