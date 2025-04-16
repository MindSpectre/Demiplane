#pragma once
#include <functional>
#include <chrono>

#include "thread_pool.hpp"


namespace demiplane::multithread {
    class ThreadPool;
    class EnqueuedTask {
    public:
        void execute() {
            last_active_time_ = std::chrono::steady_clock::now();
            task();
        }
        void set_priority(const uint32_t priority) {
            priority_ = priority;
        }
        [[nodiscard]] uint32_t priority() const {
            return priority_;
        }

        [[nodiscard]] std::chrono::time_point<std::chrono::steady_clock> last_execute() const {
            return last_active_time_;
        }
        EnqueuedTask(const uint32_t priority, std::function<void()> task)
            : priority_{priority}, task{std::move(task)} {
            last_active_time_ = std::chrono::steady_clock::now();
        }
        EnqueuedTask() {
            last_active_time_ = std::chrono::steady_clock::now();
        }

    private:
        uint32_t priority_{1};
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
}

