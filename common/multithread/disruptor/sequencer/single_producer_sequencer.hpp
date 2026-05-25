#pragma once

#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <thread>
#include <utility>

#include <pause.hpp>

#include "sequence.hpp"
#include "sequencer_concept.hpp"
#include "shared/constants.hpp"
#include "wait_strategies/wait_strategy.hpp"

namespace demiplane::multithread {

    /**
     * @brief Single-producer / single-consumer sequencer — the SPSC fast path.
     *
     * Because exactly one producer claims and publishes **in order**, two costs of the
     * multi-producer path disappear:
     *   - no CAS / fetch-add on claim — the claimed counter (`next_value_`) is a plain
     *     producer-private long; only the published `cursor_` is atomic;
     *   - no per-slot availability buffer — there can be no gaps, so the cursor *is* the
     *     published frontier and `get_highest_published()` simply returns its upper bound.
     *
     * Contract: a single producer thread calls next()/next_batch()/try_next()/publish()
     * and must publish in claim order. Using it from multiple producers is undefined.
     *
     * @tparam WaitStrategyT Wait strategy (stored by value, statically dispatched — no vtable).
     */
    template <IsWaitStrategy WaitStrategyT>
    class SingleProducerSequencer {
    public:
        /**
         * @brief Construct over a runtime (power-of-2) buffer size.
         * @param buffer_size Ring size; must be a non-zero power of 2 (debug-asserted).
         * @param wait_strategy_args Forwarded to construct the wait strategy in place.
         */
        template <typename... WaitStrategyArgsTp>
        explicit SingleProducerSequencer(const std::size_t buffer_size,
                                         WaitStrategyArgsTp&&... wait_strategy_args) noexcept
            : buffer_size_{buffer_size},
              wait_strategy_{std::forward<WaitStrategyArgsTp>(wait_strategy_args)...} {
            assert(buffer_size != 0 && std::has_single_bit(buffer_size) && "Buffer size must be a non-zero power of 2");
        }

        // ----------------------------------------------------------------- claim
        /// @brief Claim the next sequence (blocks if the buffer is full).
        [[nodiscard]] std::int64_t next() {
            const std::int64_t next = next_value_ + 1;
            wait_for_capacity(next);
            next_value_ = next;
            return next;
        }

        /// @brief Claim n contiguous sequences; returns the first.
        [[nodiscard]] std::int64_t next_batch(const std::int64_t n) {
            const std::int64_t next = next_value_ + n;  // highest claimed in the block
            wait_for_capacity(next);
            const std::int64_t first = next_value_ + 1;
            next_value_              = next;
            return first;
        }

        /// @brief Best-effort single claim; returns -1 if the buffer is full (no CAS needed).
        [[nodiscard]] std::int64_t try_next() noexcept {
            const std::int64_t next = next_value_ + 1;
            if (next - static_cast<std::int64_t>(buffer_size_) > gating_sequence_.get()) {
                return -1;  // would block
            }
            next_value_ = next;
            return next;
        }

        // --------------------------------------------------------------- publish
        /// @brief Mark a sequence published (release): its data is now visible.
        void publish(const std::int64_t sequence) noexcept {
            cursor_.set(sequence);  // release store; in-order ⇒ cursor == published frontier
            wait_strategy_.signal();
        }
        /// @brief Mark an inclusive range [lo, hi] published (in-order ⇒ just advance to hi).
        void publish_batch([[maybe_unused]] const std::int64_t lo, const std::int64_t hi) noexcept {
            cursor_.set(hi);
            wait_strategy_.signal();
        }

        // -------------------------------------------------------------- consumer
        /// @brief Highest published sequence in [lower_bound, available]. No gaps are
        /// possible with a single in-order producer, so this is just `available`.
        [[nodiscard]] std::int64_t get_highest_published([[maybe_unused]] const std::int64_t lower_bound,
                                                         const std::int64_t available_sequence) const noexcept {
            gears::force_non_static(this);
            return available_sequence;
        }

        [[nodiscard]] bool is_available(const std::int64_t sequence) const noexcept {
            return cursor_.get() >= sequence;
        }

        /// @brief Advance the consumer's gating position (drives producer backpressure).
        void update_gating_sequence(const std::int64_t sequence) noexcept {
            gating_sequence_.set(sequence);
        }

        /// @brief Block (per the wait strategy) until `sequence` is published.
        [[nodiscard]] std::int64_t wait_for(const std::int64_t sequence) {
            return wait_strategy_.wait_for(sequence, cursor_);
        }
        /// @brief Wake all waiters (e.g. for shutdown).
        void signal_all() noexcept {
            wait_strategy_.signal_all();
        }

        // ------------------------------------------------------------ accessors
        [[nodiscard]] std::int64_t get_cursor() const noexcept {
            return cursor_.get();  // = highest published (single in-order producer)
        }
        [[nodiscard]] std::int64_t get_gating_sequence() const noexcept {
            return gating_sequence_.get();
        }
        /// @brief Free slots. Computed from the atomic published cursor (not the
        /// producer-private claimed counter) so it is race-free from any thread; the
        /// slight under-estimate vs. claimed is harmless.
        [[nodiscard]] std::int64_t remaining_capacity() const noexcept {
            return static_cast<std::int64_t>(buffer_size_) - (cursor_.get() - gating_sequence_.get());
        }

    private:
        void wait_for_capacity(const std::int64_t sequence) {
            if (const std::int64_t wrap_point = sequence - static_cast<std::int64_t>(buffer_size_);
                wrap_point > cached_gating_) {  // fast path: cached gate already clears us
                std::int64_t gating;
                std::int16_t spin_count = 0;
                while (wrap_point > (gating = gating_sequence_.get())) {
                    if (++spin_count < SPIN_BEFORE_YIELD) {
                        pause_arc_agnostic();
                    } else {
                        std::this_thread::yield();
                        spin_count = 0;
                    }
                }
                cached_gating_ = gating;
            }
        }

        Sequence cursor_;                 // highest published sequence (-1 = none), atomic
        Sequence gating_sequence_;        // highest consumed sequence (-1 = none), atomic
        std::int64_t next_value_{-1};     // highest claimed sequence — producer-private, plain
        std::int64_t cached_gating_{-1};  // cached gating snapshot — producer-private, plain
        std::size_t buffer_size_;
        WaitStrategyT wait_strategy_;
    };

    static_assert(IsSequencer<SingleProducerSequencer<AnyWaitStrategy>>,
                  "SingleProducerSequencer must satisfy the Sequencer concept");

}  // namespace demiplane::multithread
