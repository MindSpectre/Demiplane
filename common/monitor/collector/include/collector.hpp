#pragma once
#include <condition_variable>
#include <functional>
#include <future>
#include <thread>

#include "stats.hpp"


namespace demiplane::monitor {
    template <typename T>
    class Collector {
    public:
        void observe(T& pobject) {
            object = pobject;
        }
        void run() {
            collector_thread_ = std::thread(&Collector<T>::collection, this);
        }
        void stop() {
            is_run = false;
        }
        void force_check() {
            checker_cv_.notify_one();
        }
        void set_timeout(const std::chrono::milliseconds timeout) {
            timeout_ = timeout;
        }

    private:
        void collection() {
            while (true) {
                std::unique_lock lock(mutex_);
                checker_cv_.wait_for(lock, timeout_);
                if (!is_run)
                    break;
                JsonStats stats = object.get().take_stats();
            }
        }
        std::atomic<bool> is_run{false};
        std::mutex mutex_;
        std::condition_variable checker_cv_;
        std::chrono::milliseconds timeout_{5000};
        std::jthread collector_thread_;
        std::reference_wrapper<T> object;
    };
}  // namespace demiplane::monitor
