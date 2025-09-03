
#pragma once
#include <functional>

namespace demiplane::multithread {
    class ThreadPool;
    class EnqueuedTask {
    public:
        void execute() const {
            if (task) {
                task();
            }
        }

        EnqueuedTask(std::function<void()> task, const uint32_t priority)
            : task{std::move(task)},
              priority_{priority} {
        }

    private:
        std::function<void()> task;
        uint32_t priority_{1};

        friend bool operator<(const EnqueuedTask& lhs, const EnqueuedTask& rhs) {
            return lhs.priority_ < rhs.priority_;  // Lower priority values have higher priority in queue
        }

        friend bool operator<=(const EnqueuedTask& lhs, const EnqueuedTask& rhs) {
            return !(lhs > rhs);
        }

        friend bool operator>(const EnqueuedTask& lhs, const EnqueuedTask& rhs) {
            return lhs.priority_ > rhs.priority_;
        }

        friend bool operator>=(const EnqueuedTask& lhs, const EnqueuedTask& rhs) {
            return !(lhs < rhs);
        }
    };
}  // namespace demiplane::multithread
