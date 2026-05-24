#pragma once

#include <atomic>
#include <cassert>
#include <cstdint>

#include <gears_class_traits.hpp>

#include "detail/waiter_list.hpp"

namespace demiplane::multithread {
    /**
     * @brief Move-only RAII handle for an AsyncResourcePool free slot.
     *
     * Identical to @c Lease<T> except its release wakes a coroutine waiter
     * (@c detail::WaiterList::wake_one) rather than an @c EventCount. Capacity-erased:
     * the type is exactly @c AsyncLease<T>. Must not outlive its pool.
     */
    template <typename T>
    class AsyncLease : gears::NonCopyable {
    public:
        using value_type = T;

        AsyncLease() noexcept = default;

        AsyncLease(T* resource,
                   std::atomic<std::uint64_t>* word,
                   const std::uint64_t bit,
                   detail::WaiterList* waiters) noexcept
            : resource_{resource},
              word_{word},
              bit_{bit},
              waiters_{waiters} {
        }

        AsyncLease(AsyncLease&& other) noexcept
            : resource_{other.resource_},
              word_{other.word_},
              bit_{other.bit_},
              waiters_{other.waiters_} {
            other.word_ = nullptr;
        }

        AsyncLease& operator=(AsyncLease&& other) noexcept {
            if (this != &other) {
                release();
                resource_   = other.resource_;
                word_       = other.word_;
                bit_        = other.bit_;
                waiters_    = other.waiters_;
                other.word_ = nullptr;
            }
            return *this;
        }

        ~AsyncLease() {
            release();
        }

        [[nodiscard]] T& operator*() const noexcept {
            assert(word_ != nullptr && "dereferencing a moved-from AsyncLease");
            return *resource_;
        }

        [[nodiscard]] T* operator->() const noexcept {
            assert(word_ != nullptr && "dereferencing a moved-from AsyncLease");
            return resource_;
        }

        [[nodiscard]] T* get() const noexcept {
            return resource_;
        }

    private:
        void release() noexcept {
            if (word_ != nullptr) {
                word_->fetch_or(bit_, std::memory_order_release);
                waiters_->wake_one();
                word_ = nullptr;
            }
        }

        T* resource_                      = nullptr;
        std::atomic<std::uint64_t>* word_ = nullptr;
        std::uint64_t bit_                = 0;
        detail::WaiterList* waiters_      = nullptr;
    };
}  // namespace demiplane::multithread
