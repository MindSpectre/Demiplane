#pragma once

#include <array>
#include <atomic>
#include <bit>
#include <cassert>
#include <chrono>
#include <concepts>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <thread>
#include <utility>
#include <variant>
#include <vector>

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/as_tuple.hpp>
#include <boost/asio/associated_cancellation_slot.hpp>
#include <boost/asio/async_result.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/cancellation_type.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <gears_class_traits.hpp>
#include <pause.hpp>

#include "async_lease.hpp"

namespace demiplane::multithread {


    /// A factory yields a @c T for a given slot index, or ignores the index.
    /// (Async-pool copy; named distinctly from resource_pool.hpp's ResourceFactory
    /// to stay ODR-safe when both headers are included together.)
    template <typename F, typename T>
    concept AsyncResourceFactory =
        (std::invocable<F, std::size_t> && std::convertible_to<std::invoke_result_t<F, std::size_t>, T>) ||
        (std::invocable<F> && std::convertible_to<std::invoke_result_t<F>, T>);

    /**
     * @brief A bounded pool of @p MaxSize interchangeable @c T resources for coroutine
     *        callers — the async sibling of @c ResourcePool.
     *
     * Same lock-free bitset fast path, pinned region, and repair protocol as
     * @c ResourcePool, but a caller that finds no free slot suspends a coroutine
     * (via @c async_acquire_for / @c async_acquire) instead of parking a thread.
     * A given instance serves a single waiter population (coroutines), so there is
     * no sync/async arbitration.
     *
     * Outstanding @c AsyncLease handles must not outlive the pool, and @c shutdown()
     * must drain any parked waiters before destruction.
     *
     * @note A parked @c async_acquire / @c async_acquire_for honors per-operation
     *       cancellation, but only for cancellation types its coroutine's cancellation
     *       state actually delivers. asio's default filter is @c terminal, so a caller
     *       emitting only @c cancellation_type::total against a default-filtered coroutine
     *       will not cancel the wait; emit @c terminal (or @c all) instead.
     *
     * @warning **Run the acquiring coroutine on a strand if the io_context has more than
     *          one thread.** @c async_acquire_for composes a @c steady_timer timeout with
     *          the wait (via @c awaitable_operators), and neither a timer nor that
     *          composition is thread-safe; a cross-thread wake racing the timer on a bare
     *          multi-threaded executor is undefined behavior. Spawn each acquiring
     *          coroutine on its own @c boost::asio::make_strand(...) (the standard asio
     *          composed-operation requirement). A single-threaded @c io_context is fine
     *          as-is.
     */
    template <typename T, std::size_t MaxSize>
    class AsyncResourcePool : gears::Immutable {
        static_assert(MaxSize > 0, "AsyncResourcePool MaxSize must be non-zero");
        static_assert(std::move_constructible<T> || std::copy_constructible<T>,
                      "AsyncResourcePool<T>: T must be move- or copy-constructible");

        static constexpr std::size_t bits_per_word = 64;

    public:
        using value_type = T;

        static constexpr std::size_t max_size   = MaxSize;
        static constexpr std::size_t word_count = (MaxSize + bits_per_word - 1) / bits_per_word;

        template <typename Factory>
            requires AsyncResourceFactory<Factory&, T>
        constexpr explicit AsyncResourcePool(const std::size_t n_free,
                                             const std::chrono::nanoseconds spin_budget,
                                             Factory&& make)
            : AsyncResourcePool{std::size_t{0}, n_free, spin_budget, std::forward<Factory>(make)} {
        }

        template <typename Factory>
            requires AsyncResourceFactory<Factory&, T>
        constexpr explicit AsyncResourcePool(const std::size_t n_free, const std::uint32_t spin_budget, Factory&& make)
            : AsyncResourcePool{
                  std::size_t{0}, n_free, std::chrono::nanoseconds{spin_budget}, std::forward<Factory>(make)} {
        }

        template <typename Factory>
            requires AsyncResourceFactory<Factory&, T>
        constexpr explicit AsyncResourcePool(const std::size_t n_free, Factory&& make)
            : AsyncResourcePool{std::size_t{0}, n_free, std::chrono::nanoseconds{100}, std::forward<Factory>(make)} {
        }

        template <typename Factory>
            requires AsyncResourceFactory<Factory&, T>
        constexpr explicit AsyncResourcePool(const std::size_t n_pinned,
                                             const std::size_t n_free,
                                             const std::chrono::nanoseconds spin_budget,
                                             Factory&& make)
            : n_pinned_{n_pinned},
              n_free_{n_free},
              n_free_words_{(n_free + bits_per_word - 1) / bits_per_word},
              spin_budget_{spin_budget} {
            if (n_pinned + n_free > MaxSize) {
                throw std::invalid_argument{"AsyncResourcePool: n_pinned + n_free exceeds MaxSize"};
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
            for (std::size_t w = 0; w < n_free_words_; ++w) {
                const std::size_t base  = w * bits_per_word;
                const std::size_t count = (n_free_ - base < bits_per_word) ? n_free_ - base : bits_per_word;
                const std::uint64_t mask =
                    (count >= bits_per_word) ? ~std::uint64_t{0} : ((std::uint64_t{1} << count) - 1);
                free_words_[w].bits.store(mask, std::memory_order_relaxed);
            }
        }

        ~AsyncResourcePool() noexcept {
            assert(waiters_.empty() && "AsyncResourcePool destroyed with parked waiters; call shutdown() first");
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

        /// Non-suspending fast path. Lock-free bitset CAS scan; @c std::nullopt if no
        /// free slot is available.
        [[nodiscard]] std::optional<AsyncLease<T>> try_acquire() noexcept {
            if (n_free_words_ == 0) {
                return std::nullopt;
            }
            const std::size_t start = word_hint() % n_free_words_;
            for (std::size_t k = 0; k < n_free_words_; ++k) {
                const std::size_t w              = (start + k) % n_free_words_;
                std::atomic<std::uint64_t>& word = free_words_[w].bits;
                std::uint64_t cur                = word.load(std::memory_order_relaxed);
                while (cur != 0) {
                    if (const std::uint64_t bit = cur & (~cur + 1); word.compare_exchange_weak(
                            cur, cur & ~bit, std::memory_order_acquire, std::memory_order_relaxed)) {
                        const std::size_t free_idx =
                            w * bits_per_word + static_cast<std::size_t>(std::countr_zero(bit));
                        return AsyncLease<T>{slot_ptr(n_pinned_ + free_idx), &word, bit, &waiters_};
                    }
                }
            }
            return std::nullopt;
        }

        /// Access the pinned cell for index @p i (Tier 1, zero contention).
        [[nodiscard]] std::atomic<T*>& pinned(const std::size_t i) noexcept {
            assert(i < n_pinned_ && "AsyncResourcePool::pinned index out of range");
            return pinned_cells_[i];
        }

        /// Try to claim free slot @p i for repair, racing acquirers.
        [[nodiscard]] bool try_claim_free_for_repair(const std::size_t i) noexcept {
            assert(i < n_free_ && "try_claim_free_for_repair index out of range");
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

        /// Return free slot @p i to circulation (sets its bit, wakes one waiter).
        void mark_healthy_free(const std::size_t i) noexcept {
            assert(i < n_free_ && "mark_healthy_free index out of range");
            const std::uint64_t bit = std::uint64_t{1} << (i % bits_per_word);
            free_words_[i / bits_per_word].bits.fetch_or(bit, std::memory_order_release);
            waiters_.wake_one();
        }

        /// Take a free slot, suspending the coroutine up to @p timeout. @c std::nullopt
        /// on timeout / cancellation / shutdown.
        [[nodiscard]] boost::asio::awaitable<std::optional<AsyncLease<T>>>
        async_acquire_for(const boost::asio::any_io_executor exec, const std::chrono::nanoseconds timeout) {
            using namespace boost::asio::experimental::awaitable_operators;
            if (auto lease = try_acquire()) {
                co_return lease;
            }
            const auto t0       = std::chrono::steady_clock::now();
            const auto deadline = t0 + timeout;
            const auto spin_end = t0 + spin_budget_;
            while (std::chrono::steady_clock::now() < (spin_end < deadline ? spin_end : deadline)) {
                pause_arc_agnostic();
                if (auto lease = try_acquire()) {
                    co_return lease;
                }
            }

            detail::WaiterNode node;
            boost::asio::steady_timer timer{exec};
            timer.expires_at(deadline);
            for (;;) {
                if (std::chrono::steady_clock::now() >= deadline) {
                    co_return std::nullopt;
                }
                auto which = co_await (async_park(node, exec, boost::asio::use_awaitable) ||
                                       timer.async_wait(boost::asio::as_tuple(boost::asio::use_awaitable)));
                if (which.index() == 1) {
                    co_return std::nullopt;  // timer expired first => timeout
                }
                if (std::get<0>(which) != detail::WaitOutcome::woken) {
                    co_return std::nullopt;  // cancelled or shut down
                }
                if (auto lease = try_acquire()) {
                    co_return lease;
                }
                // Woken but lost the CAS race to another acquirer — re-park.
            }
        }

        /// Take a free slot, suspending the coroutine indefinitely. Resolves only on a
        /// release (then retried), cancellation, or shutdown.
        [[nodiscard]] boost::asio::awaitable<std::optional<AsyncLease<T>>>
        async_acquire(const boost::asio::any_io_executor exec) {
            if (auto lease = try_acquire()) {
                co_return lease;
            }
            const auto t0       = std::chrono::steady_clock::now();
            const auto spin_end = t0 + spin_budget_;
            while (std::chrono::steady_clock::now() < spin_end) {
                pause_arc_agnostic();
                if (auto lease = try_acquire()) {
                    co_return lease;
                }
            }
            detail::WaiterNode node;
            for (;;) {
                if (const detail::WaitOutcome why = co_await async_park(node, exec, boost::asio::use_awaitable);
                    why != detail::WaitOutcome::woken) {
                    co_return std::nullopt;  // cancelled or shut down
                }
                if (auto lease = try_acquire()) {
                    co_return lease;
                }
            }
        }

        /// Graceful drain: complete every parked waiter with nullopt, and make
        /// subsequent parking acquires bail to nullopt. The bitset is untouched —
        /// try_acquire still works on any free slots. Idempotent.
        void shutdown() noexcept {
            shutdown_.store(true, std::memory_order_release);
            waiters_.drain_all(detail::WaitState::shut_down);
        }

    private:
        // Suspends the calling coroutine until a release wakes this node, or the
        // operation is cancelled (timeout via the || group, or a parent cancellation),
        // or the pool is shut down. asio owns suspend/resume + executor hop + the
        // cancellation slot; the frame-local `node` is never heap-allocated.
        template <typename CompletionToken>
        auto async_park(detail::WaiterNode& node, boost::asio::any_io_executor exec, CompletionToken&& token) {
            return boost::asio::async_initiate<CompletionToken, void(detail::WaitOutcome)>(
                [this, &node, exec]<typename Handler>(Handler&& handler) mutable {
                    this->initiate_park(node, std::move(exec), std::forward<Handler>(handler));
                },
                token);
        }

        template <typename Handler>
        void initiate_park(detail::WaiterNode& node, const boost::asio::any_io_executor& exec, Handler&& handler) {
            node.state.store(detail::WaitState::parked, std::memory_order_relaxed);
            // Resume on the executor the caller passed (== the coroutine's executor). The
            // use_awaitable completion handler does its own hop to the awaiting coroutine's
            // executor on resume, so posting here to `exec` is correct and cheap.
            node.exec = exec;

            // Read the cancellation slot from the LOCAL `handler` parameter, before moving
            // it into node.handler and before link() publishes the node. Once published, a
            // concurrent wake_one/complete may move node.handler under the WaiterList mutex
            // — reading node.handler here (unsynchronized) would race that move.
            auto slot    = boost::asio::get_associated_cancellation_slot(handler);
            node.handler = std::forward<Handler>(handler);

            // Install the cancellation handler (timeout via ||, or a parent cancel) BEFORE
            // publishing. After link(), another thread may complete this node and tear down
            // its cancellation state, so the slot must be wired while we still exclusively
            // own the node. A cancellation cannot fire during this synchronous setup (the
            // coroutine has not yet yielded to any canceller), so complete() here always
            // runs after link().
            if (slot.is_connected()) {
                slot.assign([this, &node](boost::asio::cancellation_type) {
                    waiters_.complete(node, detail::WaitState::cancelled);
                });
            }

            waiters_.link(node);  // publish: a concurrent release can now find us

            // If the pool shut down before/while we linked, bail now. drain_all may also
            // be completing this node concurrently; the state CAS in complete() makes
            // whichever lands first the sole winner.
            if (shutdown_.load(std::memory_order_acquire)) {
                waiters_.complete(node, detail::WaitState::shut_down);
                return;
            }

            // Close the lost-wakeup gap: a release between the coroutine's last
            // try_acquire and link() set a bit we may not have seen. link() took the
            // WaiterList mutex; a releasing wake_one also takes it AFTER its
            // fetch_or(release), so this scan is correctly ordered after any such
            // release and observes the freed bit.
            if (has_free_bit()) {
                waiters_.complete(node, detail::WaitState::notified);
            }
        }

        /// Relaxed scan for any free bit. Used by the park recheck to close the
        /// lost-wakeup gap; correctness rests on the WaiterList mutex providing the
        /// happens-before edge with a concurrent release.
        [[nodiscard]] bool has_free_bit() const noexcept {
            for (std::size_t w = 0; w < n_free_words_; ++w) {
                if (free_words_[w].bits.load(std::memory_order_relaxed) != 0) {
                    return true;
                }
            }
            return false;
        }

        struct alignas(64) PaddedWord {
            std::atomic<std::uint64_t> bits{0};
        };

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

        /// Per-thread start-word hint (private static; ODR-distinct from the sync pool's).
        [[nodiscard]] static std::size_t word_hint() noexcept {
            const thread_local std::size_t hint = std::hash<std::thread::id>{}(std::this_thread::get_id());
            return hint;
        }

        const std::size_t n_pinned_;
        const std::size_t n_free_;
        const std::size_t n_free_words_;

        std::array<Slot, MaxSize> storage_{};
        alignas(64) std::array<std::atomic<T*>, MaxSize> pinned_cells_{};
        std::array<PaddedWord, word_count> free_words_{};
        detail::WaiterList waiters_{};
        std::atomic<bool> shutdown_{false};
        std::chrono::nanoseconds spin_budget_;  // 100 ns
    };

}  // namespace demiplane::multithread
