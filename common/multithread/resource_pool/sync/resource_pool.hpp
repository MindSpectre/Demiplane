#pragma once

#include <array>
#include <atomic>
#include <bit>
#include <cassert>
#include <chrono>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <thread>
#include <utility>

#include <event_count.hpp>
#include <gears_class_traits.hpp>
#include <pause.hpp>

#include "lease.hpp"

namespace demiplane::multithread {

    /// A factory yields a @c T for a given slot index, or ignores the index.
    template <typename F, typename T>
    concept ResourceFactory =
        (std::invocable<F, std::size_t> && std::convertible_to<std::invoke_result_t<F, std::size_t>, T>) ||
        (std::invocable<F> && std::convertible_to<std::invoke_result_t<F>, T>);

    namespace detail {
        /// Per-thread start-word hint, seeded once, to spread bitset CAS contention.
        inline std::size_t resource_pool_word_hint() noexcept {
            const thread_local std::size_t hint = std::hash<std::thread::id>{}(std::this_thread::get_id());
            return hint;
        }
    }  // namespace detail

    /**
     * @brief A bounded pool of @p MaxSize interchangeable @c T resources, partitioned at
     *        construction into exclusively-owned @e pinned slots and a shared @e free region.
     *
     * Storage is entirely inline (no heap). Pinned slots give one designated thread
     * zero-overhead exclusive access via @c pinned(i); free slots are acquired with a
     * lock-free bitset scan (@c try_acquire) or a bounded wait (@c acquire_for) and handed
     * out as move-only @c Lease<T> objects that release on destruction.
     *
     * ## Thread safety
     *
     * - @c try_acquire and @c acquire_for are safe to call from any number of threads
     *   concurrently.
     * - @c try_claim_free_for_repair and @c mark_healthy_free are safe to interleave with
     *   concurrent acquirers (typically called from a dedicated repair/janitor thread).
     * - @c pinned(i) returns a stable @c atomic reference; the cooperative pointer-swap
     *   repair protocol assumes the slot's designated owner thread does load-only access
     *   and a single repair thread does the @c exchange / @c store cycle.
     * - Outstanding @c Lease<T> handles must not outlive the pool — destroy / release
     *   them before the @c ResourcePool destructor runs.
     * - Construction and destruction of the @c ResourcePool itself are NOT thread-safe.
     *
     * @tparam T        the pooled resource type (need not be default-constructible).
     * @tparam MaxSize  the inline capacity cap; @c n_pinned + @c n_free must not exceed it.
     */
    template <typename T, std::size_t MaxSize>
    class ResourcePool : gears::Immutable {
        static_assert(MaxSize > 0, "ResourcePool MaxSize must be non-zero");
        static_assert(std::move_constructible<T> || std::copy_constructible<T>,
                      "ResourcePool<T>: T must be move- or copy-constructible "
                      "(the factory's return value is constructed into the slot)");

        static constexpr std::size_t bits_per_word = 64;

    public:
        using value_type = T;

        static constexpr std::size_t max_size   = MaxSize;
        static constexpr std::size_t word_count = (MaxSize + bits_per_word - 1) / bits_per_word;

        /// Construct a free-only pool (n_pinned == 0) — the common case.
        template <typename Factory>
            requires ResourceFactory<Factory&, T>
        explicit ResourcePool(const std::size_t n_free, const std::chrono::nanoseconds spin_budget, Factory&& make)
            : ResourcePool{std::size_t{0}, n_free, spin_budget, std::forward<Factory>(make)} {
        }

        template <typename Factory>
            requires ResourceFactory<Factory&, T>
        constexpr explicit ResourcePool(const std::size_t n_free, const std::uint32_t spin_budget, Factory&& make)
            : ResourcePool{std::size_t{0}, n_free, std::chrono::nanoseconds{spin_budget}, std::forward<Factory>(make)} {
        }

        template <typename Factory>
            requires ResourceFactory<Factory&, T>
        constexpr explicit ResourcePool(const std::size_t n_free, Factory&& make)
            : ResourcePool{std::size_t{0}, n_free, std::chrono::nanoseconds{400}, std::forward<Factory>(make)} {
        }
        /// Construct a partitioned pool.
        template <typename Factory>
            requires ResourceFactory<Factory&, T>
        constexpr ResourcePool(const std::size_t n_pinned,
                               const std::size_t n_free,
                               const std::chrono::nanoseconds spin_budget,
                               Factory&& make)
            : n_pinned_{n_pinned},
              n_free_{n_free},
              n_free_words_{(n_free + bits_per_word - 1) / bits_per_word},
              spin_budget_{spin_budget} {
            if (n_pinned + n_free > MaxSize) {
                throw std::invalid_argument{"ResourcePool: n_pinned + n_free exceeds MaxSize"};
            }

            std::size_t built = 0;
            try {
                for (; built < n_pinned_ + n_free_; ++built) {
                    if constexpr (std::invocable<Factory&, std::size_t>) {
                        std::construct_at(slot_ptr(built), std::invoke(make, built));
                    } else {
                        std::construct_at(slot_ptr(built), std::invoke(make));
                    }
                }
            } catch (...) {
                for (std::size_t i = 0; i < built; i++) {
                    std::destroy_at(slot_ptr(i));
                }
                throw;
            }

            for (std::size_t i = 0; i < n_pinned_; ++i) {
                pinned_cells_[i].store(slot_ptr(i), std::memory_order_relaxed);
            }

            // Free bits [0, n_free_) start set: bit == 1 means "free AND healthy".
            for (std::size_t w = 0; w < n_free_words_; ++w) {
                const std::size_t base  = w * bits_per_word;
                const std::size_t count = (n_free_ - base < bits_per_word) ? n_free_ - base : bits_per_word;
                const std::uint64_t mask =
                    (count >= bits_per_word) ? ~std::uint64_t{0} : ((std::uint64_t{1} << count) - 1);
                free_words_[w].bits.store(mask, std::memory_order_relaxed);
            }
        }

        ~ResourcePool() noexcept {
            for (std::size_t i = 0; i < n_pinned_ + n_free_; ++i) {
                std::destroy_at(slot_ptr(i));
            }
        }

        [[nodiscard]] constexpr std::size_t capacity() const noexcept {
            return n_pinned_ + n_free_;
        }
        [[nodiscard]] constexpr std::size_t pinned_count() const noexcept {
            return n_pinned_;
        }
        [[nodiscard]] constexpr std::size_t free_count() const noexcept {
            return n_free_;
        }

        /**
         * @brief Try to take a free slot without waiting.
         * @return a Lease on success, std::nullopt if no free slot is available.
         *
         * Lock-free: a thread-local start word spreads contention; each candidate bit is
         * claimed with a CAS that flips it 1 -> 0 (acquire on success, synchronizing with
         * the release in Lease's destructor / mark_healthy_free).
         */
        [[nodiscard]] std::optional<Lease<T>> try_acquire() noexcept {
            if (n_free_words_ == 0) {
                return std::nullopt;
            }
            const std::size_t start = detail::resource_pool_word_hint() % n_free_words_;
            for (std::size_t k = 0; k < n_free_words_; ++k) {
                const std::size_t w              = (start + k) % n_free_words_;
                std::atomic<std::uint64_t>& word = free_words_[w].bits;
                std::uint64_t cur                = word.load(std::memory_order_relaxed);
                while (cur != 0) {
                    if (const std::uint64_t bit = cur & (~cur + 1); word.compare_exchange_weak(
                            cur, cur & ~bit, std::memory_order_acquire, std::memory_order_relaxed)) {
                        const std::size_t free_idx =
                            w * bits_per_word + static_cast<std::size_t>(std::countr_zero(bit));
                        return Lease<T>{slot_ptr(n_pinned_ + free_idx), &word, bit, &free_waiters_};
                    }
                    // compare_exchange_weak reloaded `cur` on failure.
                }
            }
            return std::nullopt;
        }

        /**
         * @brief Take a free slot, waiting up to @p timeout.
         * @return a Lease on success, std::nullopt if the timeout elapses first.
         *
         * Fast path (try_acquire), then a spin phase (pause_arc_agnostic) bounded by
         * min(spin_budget, deadline), then an EventCount park phase until the deadline.
         * `deadline` is derived once from `t0 + timeout` and never recomputed.
         */
        [[nodiscard]] std::optional<Lease<T>> acquire_for(const std::chrono::nanoseconds timeout) noexcept {
            if (std::optional<Lease<T>> lease = try_acquire()) {
                return lease;
            }

            const auto t0       = std::chrono::steady_clock::now();
            const auto deadline = t0 + timeout;
            const auto spin_end = t0 + spin_budget_;

            // Spin phase.
            while (std::chrono::steady_clock::now() < (spin_end < deadline ? spin_end : deadline)) {
                pause_arc_agnostic();
                if (std::optional<Lease<T>> lease = try_acquire()) {
                    return lease;
                }
            }

            // Park phase.
            for (;;) {
                const std::uint32_t key = free_waiters_.prepare_wait();
                if (std::optional<Lease<T>> lease = try_acquire()) {
                    free_waiters_.cancel_wait();
                    return lease;
                }
                if (std::chrono::steady_clock::now() >= deadline) {
                    free_waiters_.cancel_wait();
                    return std::nullopt;
                }
                free_waiters_.wait_until(key, deadline);  // consumes the registration
            }
        }

        /**
         * @brief Access the pinned cell for index @p i (Tier 1, zero contention).
         *
         * The designated owner caches this reference once and loads it with acquire on
         * every iteration — no CAS, no shared mutability beyond the pointer. The cell is
         * the synchronization point for cooperative pointer-swap repair: a repair thread
         * does exchange(nullptr) / reconstruct / store(p), and the owner skips its work
         * while the load yields nullptr.
         */
        [[nodiscard]] std::atomic<T*>& pinned(const std::size_t i) noexcept {
            assert(i < n_pinned_ && "ResourcePool::pinned index out of range");
            return pinned_cells_[i];
        }

        /**
         * @brief Try to claim free slot @p i for repair, racing acquirers.
         * @return true if the slot was free and is now claimed (its bit is 0, "down");
         *         false if a leaseholder currently has it (the caller should retry).
         *
         * Same CAS mechanism as try_acquire, aimed at one specific slot.
         */
        [[nodiscard]] bool try_claim_free_for_repair(const std::size_t i) noexcept {
            assert(i < n_free_ && "ResourcePool::try_claim_free_for_repair index out of range");
            std::atomic<std::uint64_t>& word = free_words_[i / bits_per_word].bits;
            const std::uint64_t bit          = std::uint64_t{1} << (i % bits_per_word);
            std::uint64_t cur                = word.load(std::memory_order_relaxed);
            while ((cur & bit) != 0) {
                if (word.compare_exchange_weak(cur, cur & ~bit, std::memory_order_acquire, std::memory_order_relaxed)) {
                    return true;
                }
            }
            return false;
        }

        /**
         * @brief Return free slot @p i to circulation (sets its bit, notifies one waiter).
         *
         * Used by the system thread after reconstructing a repaired slot. Identical to
         * the release performed by Lease's destructor.
         */
        void mark_healthy_free(const std::size_t i) noexcept {
            assert(i < n_free_ && "ResourcePool::mark_healthy_free index out of range");
            const std::uint64_t bit = std::uint64_t{1} << (i % bits_per_word);
            free_words_[i / bits_per_word].bits.fetch_or(bit, std::memory_order_release);
            free_waiters_.notify_one();
        }

    private:
        /// Cache-line-isolated, write-hot free-region bitset word.
        struct alignas(64) PaddedWord {
            std::atomic<std::uint64_t> bits{0};
        };

        /// Inline uninitialized aligned storage for one T. The pool placement-constructs
        /// exactly the live slots in the ctor and destroys them in the dtor, so T need
        /// not be default-constructible.
        union Slot {
            Slot() noexcept {
            }
            ~Slot() noexcept {
            }
            T value;
        };

        [[nodiscard]] T* slot_ptr(const std::size_t i) noexcept {
            return &storage_[i].value;
        }

        const std::size_t n_pinned_;
        const std::size_t n_free_;
        const std::size_t n_free_words_;

        std::array<Slot, MaxSize> storage_{};
        // alignas(64) isolates this array from the write-hot storage_/free_words_ regions.
        // Cells are intentionally NOT padded from each other: they are publish-once at
        // construction and read-mostly thereafter, so per-cell line sharing is harmless.
        alignas(64) std::array<std::atomic<T*>, MaxSize> pinned_cells_{};
        std::array<PaddedWord, word_count> free_words_{};

        /// Spin budget for acquire_for before parking on the EventCount.
        std::chrono::nanoseconds spin_budget_;
        EventCount free_waiters_{};
    };

}  // namespace demiplane::multithread
