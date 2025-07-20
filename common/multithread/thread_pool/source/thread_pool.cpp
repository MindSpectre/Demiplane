#include "thread_pool.hpp"


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
                std::unique_lock lock(task_queue_mutex_);
                task_condition_.wait_for(lock, config_.idle_timeout, [this] {
                    return stop_ || !tasks_.read()->empty();
                });

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
                    /*TODO:
                        doesnt consider minimum amount of threads
                        (so pool can be exhausted (leave 0 workers))
                    */
                    // No tasks available - check if we should exit due to idle timeout
                    if (size() > min_threads() && std::chrono::steady_clock::now() - last_activity > idle_timeout()) {
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

void demiplane::multithread::ThreadPool::start_cleanup_thread() {
    cleanup_thread_ = std::jthread([this]() {
        while (!stop_) {
            // Wait for cleanup interval or stop request
            std::unique_lock lock(cleanup_mutex_);
            cleanup_condition_.wait_for(lock, config_.cleanup_interval, [&] {
                return stop_.load();
            });

            if (stop_) break;

            // Perform cleanup
            cleanup_invalid_workers();
        }
    });
}

void demiplane::multithread::ThreadPool::cleanup_invalid_workers() {
    // Quick check if cleanup is even needed
    bool needs_cleanup = false;
    workers_.with_read_lock([&](const std::list<safe_thread>& workers) {
        needs_cleanup =
            std::any_of(workers.begin(), workers.end(), [](const safe_thread& t) {
                return !t.valid.load();
            });
    });

    if (!needs_cleanup) return;

    // Perform actual cleanup
    workers_.with_lock([](std::list<safe_thread>& workers) {
        std::erase_if(workers, [](const safe_thread& t) {
            return !t.valid.load();
        });
    });
}

void demiplane::multithread::ThreadPool::shutdown() {
    {
        std::unique_lock lock(task_queue_mutex_);
        stop_ = true;
    }
    task_condition_.notify_all();
    cleanup_condition_.notify_all();
    workers_.write()->clear();
    if (cleanup_thread_.joinable()) {
        cleanup_thread_.join();
    }

}

demiplane::multithread::ThreadPool::~ThreadPool() {
    shutdown();
}
