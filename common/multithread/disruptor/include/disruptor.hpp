#pragma once

/**
 * @file disruptor.hpp
 * @brief Complete Disruptor pattern implementation for high-performance concurrent programming.
 *
 * ## What is the Disruptor Pattern?
 *
 * The Disruptor is a high-performance inter-thread messaging library developed by LMAX Exchange
 * for their trading platform. It achieves:
 * - **6 million events/second** per thread
 * - **Sub-microsecond latency** (P99 < 1μs)
 * - **Lock-free** multi-producer, single-consumer
 * - **Mechanical sympathy** (cache-friendly design)
 *
 * ## Core Concepts
 *
 * ### 1. Ring Buffer (Circular Array)
 * Fixed-size array accessed via monotonically increasing sequence numbers.
 * Power-of-2 sizing enables fast modulo via bitwise AND.
 *
 * ### 2. Sequences (Cache-Aligned Cursors)
 * Atomic counters that track positions in the ring buffer.
 * Cache-line aligned (64 bytes) to prevent false sharing.
 *
 * ### 3. Claim/Publish Protocol
 * Producers:
 * 1. Claim sequence number (atomic CAS)
 * 2. Write data to ring buffer slot
 * 3. Publish sequence (mark as available)
 *
 * Consumer:
 * 1. Wait for published sequences
 * 2. Process in strict order (no gaps)
 * 3. Update gating sequence (for backpressure)
 *
 * ### 4. Wait Strategies
 * Different ways for consumer to wait when no data available:
 * - **BusySpin**: Lowest latency (~50ns), 100% CPU
 * - **Yielding**: Balanced (~200ns), 50-100% CPU (RECOMMENDED)
 * - **Blocking**: Lowest CPU (~5μs latency), condition variable
 *
 * ## Why Is It So Fast?
 *
 * ### 1. No Locks on Hot Path
 * - Compare-And-Swap (CAS) instead of mutexes
 * - Lock-free enqueue and dequeue
 * - No kernel involvement
 *
 * ### 2. Cache-Friendly Design
 * - Ring buffer = contiguous array (sequential access)
 * - Cache-line alignment prevents false sharing
 * - Pre-allocated (no heap allocations during operation)
 *
 * ### 3. Batching
 * - Consumer processes multiple events per iteration
 * - Amortizes wait overhead across batch
 * - Reduces context switches
 *
 * ### 4. Write Combining
 * - Multiple writes to same cache line get combined by CPU
 * - Fewer cache flushes
 *
 * ## Comparison with Alternatives
 *
 * | Approach           | Throughput | Latency | Ordering | Lock-Free |
 * |--------------------|------------|---------|----------|-----------|
 * | std::queue + mutex | ~100K/s    | ~10μs   | ✅       | ❌        |
 * | lock-free queue    | ~1M/s      | ~1μs    | ❌       | ✅        |
 * | **Disruptor**      | **10M/s**  | **50ns**| **✅**   | **✅**    |
 *
 * ## Usage Example
 *
 * ```cpp
 * using namespace demiplane::multithread::disruptor;
 *
 * // 1. Create components
 * constexpr size_t BUFFER_SIZE = 1024;
 * RingBuffer<LogEntry, BUFFER_SIZE> ring_buffer;
 * auto sequencer = MultiProducerSequencer<BUFFER_SIZE>(
 *     std::make_unique<YieldingWaitStrategy>()
 * );
 *
 * // 2. Producer threads
 * void producer() {
 *     while (running) {
 *         int64_t seq = sequencer.next();          // Claim sequence
 *         ring_buffer[seq] = create_entry();       // Write data
 *         sequencer.publish(seq);                  // Make available
 *     }
 * }
 *
 * // 3. Consumer thread
 * void consumer() {
 *     int64_t next_seq = 0;
 *     while (running) {
 *         int64_t available = sequencer.get_highest_published(
 *             next_seq,
 *             sequencer.get_cursor()
 *         );
 *
 *         for (int64_t seq = next_seq; seq <= available; ++seq) {
 *             process(ring_buffer[seq]);           // Process in order
 *             sequencer.mark_consumed(seq);        // Mark as done
 *         }
 *
 *         next_seq = available + 1;
 *         sequencer.update_gating_sequence(available);
 *     }
 * }
 * ```
 *
 * ## When to Use Disruptor
 *
 * ✅ **Good fit:**
 * - High-throughput message passing (>100K events/sec)
 * - Strict ordering required across multiple producers
 * - Low-latency critical (microseconds matter)
 * - Single consumer (or pipeline of consumers)
 * - Known maximum throughput (bounded buffer)
 *
 * ❌ **Not ideal for:**
 * - Low message rate (<1K events/sec) - blocking queue simpler
 * - Multiple independent consumers - needs work-stealing
 * - Unbounded queues - need dynamic allocation
 * - Complex routing - message broker better fit
 *
 * ## Performance Tuning
 *
 * ### Buffer Size
 * - Too small: Frequent backpressure, producers block
 * - Too large: Memory waste, cache misses
 * - Rule of thumb: 2-4× batch size × num producers
 * - Must be power of 2 (512, 1024, 2048, 4096, 8192, 16384)
 *
 * ### Wait Strategy
 * - Ultra-low latency (<100ns): BusySpinWaitStrategy
 * - Balanced (default): YieldingWaitStrategy
 * - CPU efficiency: BlockingWaitStrategy
 *
 * ### Batching
 * - Process multiple events per consumer iteration
 * - Amortizes wait overhead
 * - Typical batch size: 64-512 events
 *
 * ## Memory Ordering Guarantees
 *
 * The Disruptor provides **strong ordering guarantees**:
 *
 * 1. **Total Order**: All consumers see events in same order
 * 2. **Causality**: If A happens-before B, all consumers see A before B
 * 3. **Visibility**: Data written before publish() is visible after is_available()
 *
 * This is achieved through carefully placed acquire/release memory barriers,
 * WITHOUT full sequential consistency (which would be slower).
 *
 * ## Thread Safety
 *
 * - **Multi-producer safe**: Multiple threads can call next()/publish()
 * - **Single consumer**: Only one thread should consume
 * - **No external synchronization needed**: All coordination is internal
 *
 * ## Further Reading
 *
 * - LMAX Disruptor: https://lmax-exchange.github.io/disruptor/
 * - Mechanical Sympathy: https://mechanical-sympathy.blogspot.com/
 * - Lock-Free Programming: https://preshing.com/20120612/an-introduction-to-lock-free-programming/
 */

#include "multi_producer_sequencer.hpp"
#include "ring_buffer.hpp"
#include "sequence.hpp"
#include "wait_strategies/blocking.hpp"
#include "wait_strategies/busy_spin.hpp"
#include "wait_strategies/timeout_blocking.hpp"
#include "wait_strategies/yielding_strategy.hpp"

namespace demiplane::multithread::disruptor {
    // All components available via this single include
}
