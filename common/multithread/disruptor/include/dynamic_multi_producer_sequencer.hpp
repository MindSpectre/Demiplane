#pragma once

#include <atomic>
#include <bit>
#include <memory>
#include <thread>
#include <vector>

#include "sequence.hpp"
#include "wait_strategies/wait_strategy.hpp"

namespace demiplane::multithread {

    /**
     * @brief Multi-producer sequencer with claim/publish protocol.
     *
     * ## The Core Problem: Out-of-Order Publishing
     *
     * With multiple producer threads, claims happen in order, but publishing can be out of order:
     *
     * Time  | Thread A      | Thread B      | Cursor | Published
     * ------|---------------|---------------|--------|----------
     * T0    | claim(100)    |               | 100    | -
     * T1    |               | claim(101)    | 101    | -
     * T2    |               | publish(101)  | 101    | 101 ❌ GAP!
     * T3    | publish(100)  |               | 101    | 100,101 ✅
     *
     * Consumer MUST wait for 100 before processing 101, even though 101 was published first!
     *
     * ## Solution: Available Flags Array
     *
     * Track which sequences have been published:
     * ```
     * available_[sequence & index_mask_] = true;  // Mark as published
     * ```
     *
     * Consumer scans for first gap:
     * ```
     * for (seq = 100; seq <= 101; seq++) {
     *     if (!available_[seq & MASK]) return seq - 1;  // Gap at seq, return previous
     * }
     * return 101;  // All available
     * ```
     *
     * ## Memory Layout (buffer_size_ = 8)
     *
     * ```
     * Cursor: [    105    ]  (atomic, cache-aligned)
     *
     * Available: [T][T][F][T][T][T][F][T]  (array of atomic bools)
     *             0  1  2  3  4  5  6  7
     *             |  |  |  |  |  |  |  |
     *          seq%8 gives index
     *
     * Sequence 105: available_[105 & 7] = available_[1] = true ✅
     * Sequence 106: available_[106 & 7] = available_[2] = false ❌ GAP!
     * Sequence 110: available_[110 & 7] = available_[6] = false ❌ GAP!
     * ```
     *
     * ## Backpressure: What Happens When Buffer is Full?
     *
     * ```
     * RingBuffer size: 1024
     * Cursor at: 2000
     * Consumer at: 977
     * Next claim would be: 2001
     *
     * Check: 2001 - 1024 = 977 (wraps around to consumer position!)
     * Action: WAIT for consumer to advance past 977
     * ```
     *
     * This prevents overwriting data the consumer hasn't processed yet.
     *
     */

    class DynamicMultiProducerSequencer {
    public:
        /**
         * @brief Construct sequencer with wait strategy and consumer tracking
         * @param buffer_size
         * @param wait_strategy How consumer waits (takes ownership)
         * @param initial_cursor Starting sequence (default: -1, means "nothing claimed yet")
         *
         * Note: With -1 initial value, first claimed sequence is 0.
         * Gating sequence also starts at -1, representing "nothing consumed yet".
         */
        explicit DynamicMultiProducerSequencer(const std::size_t buffer_size,
                                               std::unique_ptr<WaitStrategy> wait_strategy,
                                               const std::int64_t initial_cursor = -1)
            : buffer_size_{buffer_size},
              cursor_{initial_cursor},
              gating_sequence_{initial_cursor},
              available_flags_{buffer_size_},
              wait_strategy_{std::move(wait_strategy)} {
            if (!std::has_single_bit(buffer_size_)) {
                throw std::invalid_argument("Buffer size must be a power of 2");
            }
            // Initialize all slots as unavailable
        }

        /**
         * @brief Claim next sequence number (blocking if buffer full)
         * @return Claimed sequence number
         *
         * ## Algorithm
         *
         * 1. Read current cursor (CAS loop)
         * 2. Calculate next = current + 1
         * 3. Check backpressure: next - BufferSize > consumer?
         * 4. If yes: spin-wait for consumer to advance
         * 5. CAS cursor from current to next
         * 6. If CAS fails: retry from step 1 (another thread claimed it)
         * 7. Return claimed sequence
         *
         * ## Example Execution (buffer_size_ = 4)
         *
         * Thread A:
         * ```
         * current = cursor_.get() = 10
         * next = 11
         * wrap_point = 11 - 4 = 7
         * consumer = gating_sequence_.get() = 7  (OK, not wrapped yet)
         * CAS(10, 11) -> success
         * return 11
         * ```
         *
         * Thread B (racing with A):
         * ```
         * current = cursor_.get() = 10  (same!)
         * next = 11
         * CAS(10, 11) -> FAIL (A already changed it to 11)
         * current now updated to 11 by CAS
         * next = 12
         * CAS(11, 12) -> success
         * return 12
         * ```
         */
        [[nodiscard]] std::int64_t next() {
            std::int64_t current;
            std::int64_t next;

            do {
                // Read current cursor value
                current = cursor_.get();
                next    = current + 1;

                // Check if we're about to wrap and overwrite unconsumed data
                const std::int64_t cached_gating_seq = gating_sequence_.get();

                // Backpressure: wait for consumer to advance
                // We have buffer_size_ slots. If distance between next and gating > buffer_size_,
                // we'd overwrite unconsumed data.
                if (const std::int64_t wrap_point = next - static_cast<std::int64_t>(buffer_size_);
                    wrap_point > cached_gating_seq) {
                    // Consumer hasn't caught up - we'd overwrite data
                    std::int64_t gating_seq = cached_gating_seq;

                    // Spin until consumer advances enough
                    while (wrap_point > gating_seq) {
                        gating_seq = gating_sequence_.get();
                        std::this_thread::yield();
                    }
                }

                // Try to claim sequence via CAS
                // If another thread claimed it, current is updated to new value
            } while (!cursor_.compare_and_set(current, next));

            return next;
        }

        /**
         * @brief Try to claim next sequence (non-blocking)
         * @return Claimed sequence, or -1 if buffer full
         *
         * Use this for "best effort" scenarios where dropping is acceptable:
         * - Metrics/telemetry (losing occasional metric is OK)
         * - High-frequency updates (latest value is what matters)
         * - Overload protection (drop instead of blocking)
         *
         * Example:
         * ```
         * std::int64_t seq = sequencer.try_next();
         * if (seq != -1) {
         *     ring_buffer[seq] = data;
         *     sequencer.publish(seq);
         * } else {
         *     // Buffer full, drop or retry later
         * }
         * ```
         */
        [[nodiscard]] std::int64_t try_next() noexcept {
            std::int64_t current    = cursor_.get();
            const std::int64_t next = current + 1;

            // Check backpressure
            if (const std::int64_t wrap_point = next - static_cast<std::int64_t>(buffer_size_);
                wrap_point > gating_sequence_.get()) {
                return -1;  // Buffer full, would block
            }

            // Try single CAS (don't retry if fails)
            if (cursor_.compare_and_set(current, next)) {
                return next;
            }

            return -1;  // Another thread claimed it
        }

        /**
         * @brief Claim batch of N sequences
         * @param n Number of sequences to claim
         * @return First sequence in batch
         *
         * Claiming batch [100, 101, 102]:
         * ```
         * std::int64_t first = next_batch(3);  // returns 100
         * // Use sequences: first, first+1, first+2
         * publish_batch(first, first + 2);
         * ```
         *
         * More efficient than claiming individually (one CAS for N items).
         */
        [[nodiscard]] std::int64_t next_batch(const std::int64_t n) {
            std::int64_t current;
            std::int64_t next;

            do {
                current = cursor_.get();
                next    = current + n;  // Claim N sequences

                const std::int64_t cached_gating_seq = gating_sequence_.get();

                // Check backpressure
                if (const std::int64_t wrap_point = next - static_cast<std::int64_t>(buffer_size_);
                    wrap_point > cached_gating_seq) {
                    std::int64_t gating_seq = cached_gating_seq;
                    while (wrap_point > gating_seq) {
                        gating_seq = gating_sequence_.get();
                        std::this_thread::yield();
                    }
                }
            } while (!cursor_.compare_and_set(current, next));

            return current + 1;  // First sequence in batch
        }

        /**
         * @brief Mark sequence as published (available for consumption)
         * @param sequence Sequence number to publish
         *
         * ## Critical Memory Ordering
         *
         * ```
         * // Producer thread:
         * std::int64_t seq = next();                    // Claim sequence
         * ring_buffer[seq] = create_entry(data);        // Write data (plain store)
         * publish(seq);                                 // RELEASE barrier
         *
         * // Consumer thread:
         * if (is_available(seq)) {                      // ACQUIRE barrier
         *     Entry& e = ring_buffer[seq];              // Read data (guaranteed visible!)
         *     process(e);
         * }
         * ```
         *
         * The release-acquire pair ensures data written before publish()
         * is visible to consumer after is_available() returns true.
         */
        void publish(const std::int64_t sequence) noexcept {
            // Mark as available with release semantics
            // All writes to ring_buffer[sequence] are now visible to consumers
            available_flags_[static_cast<std::size_t>(sequence) & index_mask_].value.store(true, std::memory_order_release);

            // Wake waiting consumers
            wait_strategy_->signal();
        }

        /**
         * @brief Publish batch of sequences
         * @param lo First sequence in batch (inclusive)
         * @param hi Last sequence in batch (inclusive)
         *
         * Example:
         * ```
         * std::int64_t first = next_batch(3);
         * ring_buffer[first]     = entry1;
         * ring_buffer[first + 1] = entry2;
         * ring_buffer[first + 2] = entry3;
         * publish_batch(first, first + 2);  // Mark all as published
         * ```
         */
        void publish_batch(const std::int64_t lo, const std::int64_t hi) noexcept {
            for (std::int64_t seq = lo; seq <= hi; ++seq) {
                available_flags_[static_cast<std::size_t>(seq) & index_mask_].value.store(true, std::memory_order_release);
            }
            wait_strategy_->signal();
        }

        /**
         * @brief Get highest consecutive published sequence
         * @param lower_bound Start checking from here
         * @param available_sequence Upper bound (claimed cursor)
         * @return Highest sequence with no gaps
         *
         * ## Example Scenario
         *
         * ```
         * Claimed cursor: 105
         * Available: [100✅, 101✅, 102❌, 103✅, 104✅, 105✅]
         *
         * get_highest_published(100, 105) -> returns 101
         *
         * Why? There's a GAP at 102, so we can only consume up to 101.
         * Even though 103-105 are published, we must wait for 102 first
         * to maintain ordering!
         * ```
         *
         * ## Performance Optimization
         *
         * Common case: All sequences are published (no gaps)
         * - Worst case: Scan entire batch
         * - Typical case: Find gap quickly or validate all published
         * - Cost: O(n) where n = batch size, but each check is just atomic load
         */
        [[nodiscard]] std::int64_t get_highest_published(const std::int64_t lower_bound,
                                                         const std::int64_t available_sequence) const noexcept {
            // Scan forward looking for first gap
            for (std::int64_t seq = lower_bound; seq <= available_sequence; ++seq) {
                // Acquire load ensures we see data written before publish()
                if (!available_flags_[static_cast<std::size_t>(seq) & index_mask_].value.load(std::memory_order_acquire)) {
                    return seq - 1;  // Gap found, return previous sequence
                }
            }

            // No gaps found - all sequences are published
            return available_sequence;
        }

        /**
         * @brief Check if specific sequence is published
         * @param sequence Sequence to check
         * @return true if published and ready for consumption
         */
        [[nodiscard]] bool is_available(const std::int64_t sequence) const noexcept {
            return available_flags_[static_cast<std::size_t>(sequence) & index_mask_].value.load(std::memory_order_acquire);
        }

        /**
         * @brief Mark sequence as consumed (cleanup for reuse)
         * @param sequence Sequence that's been processed
         *
         * After consumer processes data, mark slot as available for reuse.
         * This prevents issues when sequence wraps around the ring buffer.
         *
         * Example (buffer_size_ = 8):
         * ```
         * Process seq 100 (slot 4): available_[4] = true
         * Done with seq 100: mark_consumed(100) -> available_[4] = false
         * Later, seq 108 uses slot 4: available_[4] = true (fresh publish)
         * ```
         *
         * Without cleanup, seq 108 would appear "already published"!
         */
        void mark_consumed(const std::int64_t sequence) noexcept {
            available_flags_[static_cast<std::size_t>(sequence) & index_mask_].value.store(false, std::memory_order_release);
        }

        /**
         * @brief Update consumer's position (gating sequence)
         * @param sequence What consumer has processed up to
         *
         * This is critical for backpressure! Producer checks this to know
         * how far consumer has progressed.
         */
        void update_gating_sequence(const std::int64_t sequence) noexcept {
            gating_sequence_.set(sequence);
        }

        /**
         * @brief Get current claimed cursor value
         * @return Highest claimed sequence
         */
        [[nodiscard]] std::int64_t get_cursor() const noexcept {
            return cursor_.get();
        }

        /**
         * @brief Get consumer's gating sequence
         * @return Highest consumed sequence
         */
        [[nodiscard]] std::int64_t get_gating_sequence() const noexcept {
            return gating_sequence_.get();
        }

        /**
         * @brief Get remaining capacity
         * @return Number of sequences that can be claimed without blocking
         */
        [[nodiscard]] std::int64_t remaining_capacity() const noexcept {
            const std::int64_t cursor_value = cursor_.get();
            const std::int64_t gating_value = gating_sequence_.get();
            const std::int64_t consumed     = gating_value;
            const std::int64_t produced     = cursor_value;

            return static_cast<std::int64_t>(buffer_size_) - (produced - consumed);
        }

    private:
        struct atomic_wrap {
            std::atomic<bool> value{false};
        };

        std::size_t buffer_size_ = 8192;

        std::size_t index_mask_ = buffer_size_ - 1;


        /**
         * @brief Cursor tracking next claimable sequence
         *
         * Multiple producers compete to increment this via CAS.
         * Cache-aligned to prevent false sharing.
         */
        Sequence cursor_;

        /**
         * @brief Slowest consumer's position (gate)
         *
         * Producers check this for backpressure.
         * In single-consumer case, this is just the consumer's cursor.
         * In multi-consumer case, this would be the minimum of all consumers.
         */
        Sequence gating_sequence_;

        /**
         * @brief Track which sequences have been published
         *
         * Array size = buffer_size_ (ring buffer size)
         * Each element corresponds to one ring buffer slot.
         *
         * When sequence wraps around, we reuse the same slot:
         * - Sequence 100 uses available_[100 & MASK]
         * - Sequence 100+BufferSize uses same slot (must mark_consumed first!)
         */
        std::vector<atomic_wrap> available_flags_;

        /**
         * @brief Wait strategy for consumers
         */
        std::unique_ptr<WaitStrategy> wait_strategy_;
    };

}  // namespace demiplane::multithread
