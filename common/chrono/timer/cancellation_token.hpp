#pragma once

#include <atomic>

#include <gears_class_traits.hpp>

namespace demiplane::chrono {
    class CancellationToken : gears::NonCopyable {
    public:
        void cancel() noexcept {
            flag_.store(true, std::memory_order_release);
        }

        void renew() noexcept {
            flag_.store(false, std::memory_order_release);
        }

        [[nodiscard]] bool stop_requested() const noexcept {
            return flag_.load(std::memory_order_relaxed);
        }

    private:
        std::atomic_bool flag_{false};
    };

    inline CancellationToken create_cancellation_token() {
        return CancellationToken{};
    }
}  // namespace demiplane::chrono
