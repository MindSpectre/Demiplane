#pragma once

#include "wait_strategy.hpp"

namespace demiplane::multithread {

    /**
     * @brief Timeout-based blocking strategy with configurable timeout.
     *
     * Like BlockingWaitStrategy but wakes periodically to check shutdown flags.
     * Useful for graceful shutdown without explicit signaling.
     */
    class TimeoutBlockingWaitStrategy final : public WaitStrategy {
    public:
        explicit TimeoutBlockingWaitStrategy(
            const std::chrono::milliseconds timeout = std::chrono::milliseconds{100}) noexcept
            : timeout_{timeout} {
        }

        std::int64_t wait_for(const std::int64_t sequence, const Sequence& cursor) override {
            std::int64_t available_sequence;

            if ((available_sequence = cursor.get()) >= sequence) {
                return available_sequence;
            }

            std::unique_lock lock{mutex_};
            available_sequence = cursor.get();
            while (available_sequence < sequence) {
                // Wait with timeout - returns on timeout OR notify
                cv_.wait_for(lock, timeout_);

                // Check again (might be timeout, not notify)
                available_sequence = cursor.get();
            }

            return available_sequence;
        }

        void signal() noexcept override {
            cv_.notify_one();
        }

        void signal_all() noexcept override {
            cv_.notify_all();
        }


    private:
        std::int64_t
        wait_for(const std::int64_t sequence, const Sequence& cursor, const Sequence* dependent_sequence) override {
            gears::unused_value(sequence, cursor, dependent_sequence);
            throw std::logic_error{"TimeoutBlockingWaitStrategy does not support dependent sequences"};
        }
        std::mutex mutex_;
        std::condition_variable cv_;
        std::chrono::milliseconds timeout_;
    };
}  // namespace demiplane::multithread
