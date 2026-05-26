#pragma once

#include <atomic>
#include <bit>
#include <chrono>
#include <climits>
#include <cstdint>

#if !defined(DMP_EVENT_COUNT_FORCE_FALLBACK) && defined(__linux__)
    #define DMP_EVENT_COUNT_USE_FUTEX 1
    #include <ctime>

    #include <linux/futex.h>
    #include <sys/syscall.h>
    #include <unistd.h>
#else
    #define DMP_EVENT_COUNT_USE_FUTEX 0
    #include <condition_variable>
    #include <mutex>
#endif

namespace demiplane::multithread {

    /**
     * @brief Lost-wakeup-safe park/notify primitive (folly-style).
     *
     * A single @c std::atomic<uint64_t> packs @c epoch in the high 32 bits and
     * @c waiter_count in the low 32 bits. Waiters snapshot the epoch in
     * @c prepare_wait(); a releaser advances the epoch in @c notify_one() and only
     * issues a @c FUTEX_WAKE when @c waiter_count is non-zero — so a notify that hits
     * no parked waiter costs a single @c fetch_add and no syscall.
     *
     * On Linux the wait is a raw @c FUTEX_WAIT on the epoch half of the word; on every
     * other platform (or when @c DMP_EVENT_COUNT_FORCE_FALLBACK is defined) it is a
     * @c std::mutex + @c std::condition_variable with the identical public API.
     *
     * ## Thread safety
     *
     * All public methods (@c prepare_wait, @c cancel_wait, @c wait_until, @c notify_one,
     * @c notify_all, @c get_epoch, @c get_waiter_count) are safe to call concurrently from
     * any number of threads. Any mix of waiters and releasers is supported.
     * Construction and destruction of the @c EventCount itself are NOT thread-safe.
     *
     * ## Contract
     *
     * The caller MUST re-check its real condition between @c prepare_wait() and
     * @c wait_until(). A notify landing after @c prepare_wait() returned is then always
     * observed: by the re-check, by @c wait_until seeing the advanced epoch, or by the
     * @c FUTEX_WAKE. Each @c prepare_wait() is paired with exactly one @c cancel_wait()
     * OR one @c wait_until().
     */
    class alignas(64) EventCount {
    public:
        EventCount() noexcept                  = default;
        ~EventCount()                          = default;
        EventCount(const EventCount&)            = delete;
        EventCount& operator=(const EventCount&) = delete;
        EventCount(EventCount&&)                 = delete;
        EventCount& operator=(EventCount&&)      = delete;

        /// Register as a waiter and snapshot the current epoch (the wait key).
        [[nodiscard]] std::uint32_t prepare_wait() noexcept {
            const std::uint64_t s = state_.fetch_add(1, std::memory_order_seq_cst);
            return static_cast<std::uint32_t>(s >> 32);
        }

        /// Un-register a waiter that decided not to wait after all.
        void cancel_wait() noexcept {
            state_.fetch_sub(1, std::memory_order_relaxed);
        }

        /**
         * @brief Park until the epoch leaves @p key, the deadline passes, or a spurious
         *        wake. Consumes the waiter registration (trailing @c fetch_sub(1)).
         */
        void wait_until(std::uint32_t key, std::chrono::steady_clock::time_point deadline) noexcept;

        /// Advance the epoch and wake one parked waiter (if any).
        void notify_one() noexcept { notify(1); }

        /// Advance the epoch and wake all parked waiters (if any).
        void notify_all() noexcept { notify(INT_MAX); }  // INT_MAX = FUTEX_WAKE "wake all" count

        /// Diagnostic only (relaxed) — mirrors Sequence::get_volatile().
        [[nodiscard]] std::uint32_t get_epoch() const noexcept {
            return static_cast<std::uint32_t>(state_.load(std::memory_order_relaxed) >> 32);
        }

        /// Diagnostic only (relaxed).
        [[nodiscard]] std::uint32_t get_waiter_count() const noexcept {
            return static_cast<std::uint32_t>(state_.load(std::memory_order_relaxed));
        }

    private:
        void notify(int wake_count) noexcept;

        // epoch lives in the high 32 bits, waiter_count in the low 32 bits.
        static constexpr std::uint64_t epoch_increment = std::uint64_t{1} << 32;

#if DMP_EVENT_COUNT_USE_FUTEX
        static_assert(std::endian::native == std::endian::little,
                      "EventCount futex path requires a little-endian target");
        static_assert(sizeof(std::atomic<std::uint64_t>) == sizeof(std::uint64_t),
                      "EventCount futex path assumes the atomic is layout-compatible with uint64_t");
        static_assert(std::atomic<std::uint64_t>::is_always_lock_free,
                      "EventCount futex path requires a lock-free atomic (no embedded lock)");

        // little-endian: the epoch (high 32 bits) is the second uint32 of the word.
        [[nodiscard]] std::uint32_t* epoch_addr() noexcept {
            return reinterpret_cast<std::uint32_t*>(&state_) + 1;
        }
#endif

        std::atomic<std::uint64_t> state_{0};

#if !DMP_EVENT_COUNT_USE_FUTEX
        std::mutex              mutex_{};
        std::condition_variable cv_{};
#endif
    };

#if DMP_EVENT_COUNT_USE_FUTEX

    inline void EventCount::wait_until(const std::uint32_t key,
                                       const std::chrono::steady_clock::time_point deadline) noexcept {
        const auto now = std::chrono::steady_clock::now();
        if (now < deadline
            && static_cast<std::uint32_t>(state_.load(std::memory_order_acquire) >> 32) == key) {
            const auto rel   = deadline - now;
            const auto secs  = std::chrono::duration_cast<std::chrono::seconds>(rel);
            const auto nsecs = std::chrono::duration_cast<std::chrono::nanoseconds>(rel - secs);
            timespec ts{};
            ts.tv_sec  = static_cast<std::time_t>(secs.count());
            ts.tv_nsec = static_cast<long>(nsecs.count());
            // EAGAIN (epoch already moved), ETIMEDOUT, EINTR: all fine, the caller re-checks.
            ::syscall(SYS_futex, epoch_addr(), FUTEX_WAIT_PRIVATE, key, &ts, nullptr, 0);
        }
        state_.fetch_sub(1, std::memory_order_relaxed);  // consume the registration
    }

    inline void EventCount::notify(const int wake_count) noexcept {
        const std::uint64_t prev = state_.fetch_add(epoch_increment, std::memory_order_seq_cst);
        if (static_cast<std::uint32_t>(prev) != 0) {  // waiter_count != 0
            ::syscall(SYS_futex, epoch_addr(), FUTEX_WAKE_PRIVATE, wake_count, nullptr, nullptr, 0);
        }
    }

#else  // condition_variable fallback

    inline void EventCount::wait_until(const std::uint32_t key,
                                       const std::chrono::steady_clock::time_point deadline) noexcept {
        {
            std::unique_lock lock{mutex_};
            cv_.wait_until(lock, deadline, [this, key] {
                return static_cast<std::uint32_t>(state_.load(std::memory_order_acquire) >> 32) != key;
            });
        }
        state_.fetch_sub(1, std::memory_order_relaxed);  // consume the registration
    }

    inline void EventCount::notify(const int wake_count) noexcept {
        const std::uint64_t prev = state_.fetch_add(epoch_increment, std::memory_order_seq_cst);
        if (static_cast<std::uint32_t>(prev) != 0) {  // waiter_count != 0
            // Take the mutex so a waiter mid-park cannot miss this wake: the predicate
            // state (epoch) is atomic, so the lock here is what serializes with a waiter
            // that has evaluated the predicate but not yet blocked on the cv.
            std::lock_guard lock{mutex_};
            if (wake_count == 1) {
                cv_.notify_one();
            } else {
                cv_.notify_all();
            }
        }
    }

#endif

}  // namespace demiplane::multithread
