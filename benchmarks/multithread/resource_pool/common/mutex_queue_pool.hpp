#pragma once

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

namespace bench::pool {

    template <typename T>
    class MutexQueuePool;

    template <typename T>
    class MutexLease {
    public:
        MutexLease() noexcept = default;

        MutexLease(MutexQueuePool<T>* pool, const std::size_t idx) noexcept
            : pool_(pool),
              idx_(idx) {
        }

        MutexLease(const MutexLease&)            = delete;
        MutexLease& operator=(const MutexLease&) = delete;

        MutexLease(MutexLease&& other) noexcept
            : pool_(std::exchange(other.pool_, nullptr)),
              idx_(other.idx_) {
        }

        MutexLease& operator=(MutexLease&& other) noexcept {
            if (this != &other) {
                release();
                pool_ = std::exchange(other.pool_, nullptr);
                idx_  = other.idx_;
            }
            return *this;
        }

        ~MutexLease() noexcept {
            release();
        }

        T& operator*() noexcept {
            return pool_->slot_at(idx_);
        }
        T* operator->() noexcept {
            return &pool_->slot_at(idx_);
        }

    private:
        void release() noexcept {
            if (pool_ != nullptr) {
                pool_->release_index(idx_);
                pool_ = nullptr;
            }
        }

        MutexQueuePool<T>* pool_{nullptr};
        std::size_t idx_{0};
    };

    /// Textbook bounded pool: vector of T + free-index stack + mutex + condvar.
    /// Same try_acquire / acquire_for interface as ResourcePool so the bench
    /// workload code is interchangeable.
    template <typename T>
    class MutexQueuePool {
    public:
        using value_type = T;
        using lease_type = MutexLease<T>;

        template <typename Factory>
        explicit MutexQueuePool(const std::size_t n, Factory&& make) {
            slots_.reserve(n);
            free_.reserve(n);
            for (std::size_t i = 0; i < n; ++i) {
                if constexpr (std::is_invocable_v<Factory&, std::size_t>) {
                    slots_.emplace_back(make(i));
                } else {
                    slots_.emplace_back(make());
                }
                free_.push_back(i);
            }
        }

        MutexQueuePool(const MutexQueuePool&)            = delete;
        MutexQueuePool& operator=(const MutexQueuePool&) = delete;
        MutexQueuePool(MutexQueuePool&&)                 = delete;
        MutexQueuePool& operator=(MutexQueuePool&&)      = delete;
        ~MutexQueuePool() noexcept                       = default;

        [[nodiscard]] std::optional<MutexLease<T>> try_acquire() noexcept {
            std::lock_guard lock{mtx_};
            if (free_.empty()) {
                return std::nullopt;
            }
            const std::size_t idx = free_.back();
            free_.pop_back();
            return MutexLease<T>{this, idx};
        }

        [[nodiscard]] std::optional<MutexLease<T>> acquire_for(const std::chrono::nanoseconds timeout) noexcept {
            std::unique_lock lock{mtx_};
            if (!cv_.wait_for(lock, timeout, [this] { return !free_.empty(); })) {
                return std::nullopt;
            }
            const std::size_t idx = free_.back();
            free_.pop_back();
            return MutexLease<T>{this, idx};
        }

        // Exposed for MutexLease.
        T& slot_at(const std::size_t i) noexcept {
            return slots_[i];
        }

        void release_index(const std::size_t i) noexcept {
            {
                std::lock_guard lock{mtx_};
                free_.push_back(i);
            }
            cv_.notify_one();
        }

    private:
        std::vector<T> slots_;
        std::vector<std::size_t> free_;
        std::mutex mtx_;
        std::condition_variable cv_;
    };

}  // namespace bench::pool
