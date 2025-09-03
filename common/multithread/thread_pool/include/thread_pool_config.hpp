#pragma once

namespace demiplane::multithread {

    struct ThreadPoolConfig {
        std::size_t min_threads{2};
        std::size_t max_threads{4};
        std::chrono::milliseconds idle_timeout{std::chrono::seconds{30}};
        std::chrono::milliseconds cleanup_interval{std::chrono::seconds{15}};
        bool enable_cleanup_thread{true};

        [[nodiscard]] bool ok() const {
            return min_threads > 0 && max_threads > 0 && min_threads <= max_threads;
        }

        static ThreadPoolConfig minimal() {
            return ThreadPoolConfig{1, 1, std::chrono::seconds{1}, std::chrono::seconds{1}, false};
        }

        static ThreadPoolConfig basic() {
            return ThreadPoolConfig{2, 4, std::chrono::milliseconds{500}, std::chrono::seconds{1}, true};
        }

        static ThreadPoolConfig high_performance() {
            return ThreadPoolConfig{4, 16, std::chrono::seconds{10}, std::chrono::seconds{30}, true};
        }

        static ThreadPoolConfig quick_cleanup() {
            return ThreadPoolConfig{2, 8, std::chrono::milliseconds{200}, std::chrono::milliseconds{500}, true};
        }
    };
}  // namespace demiplane::multithread
