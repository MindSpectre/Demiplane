#pragma once

#include <atomic>
#include <cassert>
#include <cstdint>

#include <event_count.hpp>
#include <gears_class_traits.hpp>

namespace demiplane::multithread {

    /**
     * @brief Move-only RAII handle for a free-pool slot.
     *
     * Holds only what release touches, so the type is genuinely @c Lease<T> — it does
     * not carry the pool's @c MaxSize. The destructor (and move-assignment of an
     * occupied lease) sets the slot's bit with release ordering and notifies one
     * waiter. A moved-from lease has a null @c word_ and releases nothing.
     *
     * @warning A @c Lease<T> holds interior pointers into the @c ResourcePool's storage
     *          and into its @c EventCount waiter. It must NOT outlive the pool it was
     *          acquired from — destroy / release every outstanding lease before the
     *          pool's destructor runs. (Holding leases in containers that outlive the
     *          pool is a use-after-free.)
     *
     * @tparam T the pooled resource type.
     */
    template <typename T>
    class Lease : gears::NonCopyable {
    public:
        using value_type = T;

        Lease() noexcept = default;

        Lease(T* resource, std::atomic<std::uint64_t>* word, const std::uint64_t bit, EventCount* waiters) noexcept
            : resource_{resource},
              word_{word},
              bit_{bit},
              waiters_{waiters} {
        }

        Lease(Lease&& other) noexcept
            : resource_{other.resource_},
              word_{other.word_},
              bit_{other.bit_},
              waiters_{other.waiters_} {
            other.word_ = nullptr;
        }

        Lease& operator=(Lease&& other) noexcept {
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

        ~Lease() {
            release();
        }

        [[nodiscard]] T& operator*() const noexcept {
            assert(word_ != nullptr && "dereferencing a moved-from Lease");
            return *resource_;
        }

        [[nodiscard]] T* operator->() const noexcept {
            assert(word_ != nullptr && "dereferencing a moved-from Lease");
            return resource_;
        }

        [[nodiscard]] T* get() const noexcept {
            return resource_;
        }

    private:
        void release() noexcept {
            if (word_ != nullptr) {
                word_->fetch_or(bit_, std::memory_order_release);
                waiters_->notify_one();
                word_ = nullptr;
            }
        }

        T* resource_                      = nullptr;
        std::atomic<std::uint64_t>* word_ = nullptr;
        std::uint64_t bit_                = 0;
        EventCount* waiters_              = nullptr;
    };

}  // namespace demiplane::multithread
