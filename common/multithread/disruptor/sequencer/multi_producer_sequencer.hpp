#pragma once

#include <atomic>
#include <bit>
#include <cassert>
#include <cstdint>
#include <thread>
#include <utility>
#include <vector>

#include <pause.hpp>

#include "sequence.hpp"
#include "sequencer_concept.hpp"
#include "shared/constants.hpp"
#include "wait_strategies/wait_strategy.hpp"

namespace demiplane::multithread {

    /**
     * @brief Multi-producer sequencer with a claim/publish protocol.
     *
     * ## Claiming: lock-free fetch-add
     *
     * `next()` advances the shared cursor with a single atomic fetch-add, so every
     * producer gets a unique sequence in one instruction with no CAS-retry under
     * contention. The cursor therefore tracks the highest *claimed* sequence and may
     * run ahead of what is actually published — consumers never read the cursor as a
     * publish frontier, they use get_highest_published() which scans the availability
     * flags. (`try_next()` keeps a single-shot CAS: its contract is to *not* claim
     * when full, which fetch-add cannot model.)
     *
     * ## Availability: rotation numbers, never cleared
     *
     * Each ring slot stores the *generation* of the last sequence published into it
     * (`sequence >> index_shift`), not a boolean. A slot is "published for seq" iff it
     * holds `generation_of(seq)`. When a slot is reused a wrap later, the publisher
     * overwrites it with the new generation, so a stale value from the previous
     * generation never looks available. This needs no consumer-side write-back
     * (no mark_consumed), so the flag cache lines flow one way (producer -> consumer).
     * Backpressure (the gating wait below) still guarantees a slot's previous occupant
     * has been consumed before it is overwritten.
     *
     * @tparam WaitStrategyT Wait strategy (stored by value, statically dispatched — no vtable).
     */
    template <IsWaitStrategy WaitStrategyT>
    class MultiProducerSequencer {
    public:
        /**
         * @brief Construct over a runtime (power-of-2) buffer size.
         * @param buffer_size Ring size; must be a non-zero power of 2 (debug-asserted).
         * @param wait_strategy_args     Forwarded to construct the wait strategy in place
         *                    (supports non-movable strategies such as Blocking).
         */
        template <typename... WaitStrategyArgsTp>
        explicit MultiProducerSequencer(const std::size_t buffer_size,
                                        WaitStrategyArgsTp&&... wait_strategy_args) noexcept
            : buffer_size_{buffer_size},
              index_mask_{buffer_size - 1},
              index_shift_{std::countr_zero(buffer_size)},
              wait_strategy_{std::forward<WaitStrategyArgsTp>(wait_strategy_args)...},
              available_flags_{buffer_size} {
            assert(buffer_size != 0 && std::has_single_bit(buffer_size) && "Buffer size must be a non-zero power of 2");
        }

        // ----------------------------------------------------------------- claim
        /// @brief Claim the next sequence (blocks if the buffer is full).
        [[nodiscard]] std::int64_t next() {
            const std::int64_t next = cursor_.increment_and_get();
            wait_for_capacity(next);
            return next;
        }

        /// @brief Claim n contiguous sequences in one fetch-add; returns the first.
        [[nodiscard]] std::int64_t next_batch(const std::int64_t n) {
            const std::int64_t last = cursor_.add_and_get(n);  // highest claimed in the block
            wait_for_capacity(last);
            return last - n + 1;
        }

        /// @brief Best-effort single claim; returns -1 if full or contended (CAS).
        [[nodiscard]] std::int64_t try_next() noexcept {
            std::int64_t current    = cursor_.get();
            const std::int64_t next = current + 1;
            if (const std::int64_t wrap_point = next - static_cast<std::int64_t>(buffer_size_);
                wrap_point > gating_sequence_.get()) {
                return -1;  // would block
            }
            if (cursor_.compare_and_set(current, next)) {
                return next;
            }
            return -1;  // another producer claimed it
        }

        // --------------------------------------------------------------- publish
        /// @brief Mark a sequence published (release): its data is now visible.
        void publish(const std::int64_t sequence) noexcept {
            set_available(sequence);
            wait_strategy_.signal();
        }
        /// @brief Mark an inclusive range [lo, hi] published.
        void publish_batch(const std::int64_t lo, const std::int64_t hi) noexcept {
            for (std::int64_t seq = lo; seq <= hi; ++seq) {
                set_available(seq);
            }
            wait_strategy_.signal();
        }

        // -------------------------------------------------------------- consumer
        /// @brief Highest consecutively-published sequence in [lower_bound, available].
        [[nodiscard]] std::int64_t get_highest_published(const std::int64_t lower_bound,
                                                         const std::int64_t available_sequence) const noexcept {
            for (std::int64_t seq = lower_bound; seq <= available_sequence; ++seq) {
                if (available_flags_[static_cast<std::size_t>(seq) & index_mask_].value.load(
                        std::memory_order_acquire) != generation_of(seq)) {
                    return seq - 1;  // first gap (slot not yet published for this generation)
                }
            }
            return available_sequence;
        }

        [[nodiscard]] bool is_available(const std::int64_t sequence) const noexcept {
            return available_flags_[static_cast<std::size_t>(sequence) & index_mask_].value.load(
                       std::memory_order_acquire) == generation_of(sequence);
        }

        /// @brief Advance the consumer's gating position (drives producer backpressure).
        void update_gating_sequence(const std::int64_t sequence) noexcept {
            gating_sequence_.set(sequence);
        }

        /// @brief Block (per the wait strategy) until `sequence` is claimed by a producer.
        [[nodiscard]] std::int64_t wait_for(const std::int64_t sequence) {
            return wait_strategy_.wait_for(sequence, cursor_);
        }
        /// @brief Wake all waiters (e.g. for shutdown).
        void signal_all() noexcept {
            wait_strategy_.signal_all();
        }

        // ------------------------------------------------------------ accessors
        [[nodiscard]] std::int64_t get_cursor() const noexcept {
            return cursor_.get();
        }
        [[nodiscard]] std::int64_t get_gating_sequence() const noexcept {
            return gating_sequence_.get();
        }
        /// @brief Free slots. May read negative: with fetch-add the cursor can race
        /// ahead of the gating sequence before producers publish/consumers catch up.
        [[nodiscard]] std::int64_t remaining_capacity() const noexcept {
            return static_cast<std::int64_t>(buffer_size_) - (cursor_.get() - gating_sequence_.get());
        }

    private:
        [[nodiscard]] std::int32_t generation_of(const std::int64_t sequence) const noexcept {
            return static_cast<std::int32_t>(sequence >> index_shift_);
        }

        void set_available(const std::int64_t sequence) noexcept {
            available_flags_[static_cast<std::size_t>(sequence) & index_mask_].value.store(generation_of(sequence),
                                                                                           std::memory_order_release);
        }

        void wait_for_capacity(const std::int64_t sequence) const {
            const std::int64_t wrap_point = sequence - static_cast<std::int64_t>(buffer_size_);
            std::int16_t spin_count       = 0;
            while (wrap_point > gating_sequence_.get()) {
                if (++spin_count < SPIN_BEFORE_YIELD) {
                    pause_arc_agnostic();
                } else {
                    std::this_thread::yield();
                    spin_count = 0;
                }
            }
        }

        // Packed (not per-line aligned): consumer only reads these, producer only
        // writes them, so they intentionally share cache lines like LMAX's int[].
        struct atomic_wrap {
            std::atomic<std::int32_t> value{-1};  // -1 = never published
        };

        Sequence cursor_;           // highest claimed sequence (-1 = none)
        Sequence gating_sequence_;  // highest consumed sequence (-1 = none)
        std::size_t buffer_size_;
        std::size_t index_mask_;    // buffer_size_ - 1
        std::int32_t index_shift_;  // log2(buffer_size_)
        WaitStrategyT wait_strategy_;
        std::vector<atomic_wrap> available_flags_;
    };

    static_assert(IsSequencer<MultiProducerSequencer<AnyWaitStrategy>>,
                  "MultiProducerSequencer must satisfy the Sequencer concept");

}  // namespace demiplane::multithread
