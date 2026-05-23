#include <atomic>
#include <barrier>
#include <cstdio>
#include <demiplane/ink>
#include <format>
#include <iostream>
#include <string>
#include <vector>

#include <disruptor.hpp>
#include <thread_pinning.hpp>

using namespace demiplane::multithread;

/*==============================================================================
 * MULTI-PRODUCER -> 1 CONSUMER, one-at-a-time (8 producers, contended CAS-free claim)
 *============================================================================*/
void mpsc_one_at_a_time_test() {
    constexpr std::int64_t BUFFER_SIZE          = 8192;
    constexpr std::int64_t NUM_PRODUCERS        = 8;
    constexpr std::int64_t ENTRIES_PER_PRODUCER = 1'000'000;
    constexpr std::int64_t TOTAL_ENTRIES        = NUM_PRODUCERS * ENTRIES_PER_PRODUCER;

    Disruptor<std::int64_t, MultiProducerSequencer, BusySpinWaitStrategy> disruptor{BUFFER_SIZE};

    std::barrier sync_point{NUM_PRODUCERS + 1};
    std::atomic running{true};

    // Single consumer thread
    std::thread consumer{[&]() {
        sync_point.arrive_and_wait();

        std::int64_t next_seq           = 0;
        std::int64_t processed          = 0;
        constexpr std::int64_t expected = TOTAL_ENTRIES;

        while (running.load(std::memory_order_acquire) || processed < expected) {
            const std::int64_t cursor = disruptor.sequencer().get_cursor();

            if (const std::int64_t available = disruptor.sequencer().get_highest_published(next_seq, cursor);
                available >= next_seq) {
                // Process batch
                for (std::int64_t seq = next_seq; seq <= available; ++seq) {
                    [[maybe_unused]] std::int64_t value = disruptor.ring_buffer()[seq];
                }

                processed += (available - next_seq + 1);
                next_seq   = available + 1;
                disruptor.sequencer().update_gating_sequence(available);
            }
        }
    }};

    // Producers - one at a time
    std::vector<std::thread> producers;
    producers.reserve(NUM_PRODUCERS);

    const auto start_time = std::chrono::steady_clock::now();

    for (std::int64_t tid = 0; tid < NUM_PRODUCERS; ++tid) {
        producers.emplace_back([&, tid]() {
            sync_point.arrive_and_wait();

            for (std::int64_t i = 0; i < ENTRIES_PER_PRODUCER; ++i) {
                const std::int64_t seq       = disruptor.sequencer().next();
                disruptor.ring_buffer()[seq] = static_cast<std::int64_t>(tid * ENTRIES_PER_PRODUCER + i);
                disruptor.sequencer().publish(seq);
            }
        });
    }

    for (auto& p : producers) {
        p.join();
    }

    running.store(false, std::memory_order_release);
    consumer.join();

    const auto elapsed       = std::chrono::steady_clock::now() - start_time;
    const double elapsed_sec = std::chrono::duration<double>(elapsed).count();
    const double throughput  = TOTAL_ENTRIES / elapsed_sec;

    const std::string body = demiplane::ink::section("")
                                 .row("Producers", NUM_PRODUCERS)
                                 .row("Consumers", 1)
                                 .row("Entries/producer", ENTRIES_PER_PRODUCER)
                                 .row("Total entries", TOTAL_ENTRIES)
                                 .row("Buffer size", BUFFER_SIZE)
                                 .row("Elapsed", std::format("{:.3f} s", elapsed_sec))
                                 .row("Throughput", std::format("{:.0f} ops/s", throughput))
                                 .row("ns per op", std::format("{:.2f} ns", 1e9 / throughput))
                                 .indent_size(1)
                                 .value_align(demiplane::ink::Align::Right)
                                 .render();

    std::cout << '\n'
              << demiplane::ink::box(body)
                     .title("Disruptor — 8P→1C (one-at-a-time)")
                     .border(demiplane::ink::border::unicode)
                     .border_style(demiplane::ink::colors::bold_magenta)
                     .terminate()
                     .render();
}

/*==============================================================================
 * MULTI-PRODUCER -> 1 CONSUMER, batched claim/publish (4 producers, batch 16)
 *============================================================================*/
void mpsc_batched_test() {
    constexpr std::int64_t BUFFER_SIZE          = 8192;
    constexpr std::int64_t NUM_PRODUCERS        = 4;
    constexpr std::int64_t ENTRIES_PER_PRODUCER = 10'000'000;
    constexpr std::int64_t TOTAL_ENTRIES        = NUM_PRODUCERS * ENTRIES_PER_PRODUCER;
    constexpr std::int64_t BATCH_SIZE           = 16;  // Claim 16 sequences at once

    Disruptor<std::int64_t, MultiProducerSequencer, BusySpinWaitStrategy> disruptor{BUFFER_SIZE};

    std::barrier sync_point{NUM_PRODUCERS + 1};
    std::atomic running{true};

    // Single consumer thread
    std::thread consumer{[&]() {
        sync_point.arrive_and_wait();

        std::int64_t next_seq           = 0;
        std::int64_t processed          = 0;
        constexpr std::int64_t expected = TOTAL_ENTRIES;

        while (running.load(std::memory_order_acquire) || processed < expected) {
            const std::int64_t cursor = disruptor.sequencer().get_cursor();

            if (const std::int64_t available = disruptor.sequencer().get_highest_published(next_seq, cursor);
                available >= next_seq) {
                // Process batch
                for (std::int64_t seq = next_seq; seq <= available; ++seq) {
                    [[maybe_unused]] std::int64_t value = disruptor.ring_buffer()[seq];
                }

                processed += (available - next_seq + 1);
                next_seq   = available + 1;
                disruptor.sequencer().update_gating_sequence(available);
            }
        }
    }};

    // Producers - batched claiming
    std::vector<std::thread> producers;
    producers.reserve(NUM_PRODUCERS);

    const auto start_time = std::chrono::steady_clock::now();

    for (std::int64_t tid = 0; tid < NUM_PRODUCERS; ++tid) {
        producers.emplace_back([&, tid]() {
            sync_point.arrive_and_wait();

            for (std::int64_t i = 0; i < ENTRIES_PER_PRODUCER; i += BATCH_SIZE) {
                // Claim batch of sequences (ONE fetch-add for entire batch!)
                const std::int64_t first_seq = disruptor.sequencer().next_batch(BATCH_SIZE);

                // Fill the batch
                for (std::int64_t j = 0; j < BATCH_SIZE && (i + j) < ENTRIES_PER_PRODUCER; ++j) {
                    const std::int64_t seq       = first_seq + j;
                    disruptor.ring_buffer()[seq] = tid * ENTRIES_PER_PRODUCER + i + j;
                }

                // Publish entire batch at once
                const std::int64_t last_seq = first_seq + static_cast<std::int64_t>(BATCH_SIZE) - 1;
                disruptor.sequencer().publish_batch(first_seq, last_seq);
            }
        });
    }

    for (auto& p : producers) {
        p.join();
    }

    running.store(false, std::memory_order_release);
    consumer.join();

    const auto elapsed       = std::chrono::steady_clock::now() - start_time;
    const double elapsed_sec = std::chrono::duration<double>(elapsed).count();
    const double throughput  = TOTAL_ENTRIES / elapsed_sec;

    const std::string body = demiplane::ink::section("")
                                 .row("Producers", NUM_PRODUCERS)
                                 .row("Consumers", 1)
                                 .row("Batch size", BATCH_SIZE)
                                 .row("Entries/producer", ENTRIES_PER_PRODUCER)
                                 .row("Total entries", TOTAL_ENTRIES)
                                 .row("Buffer size", BUFFER_SIZE)
                                 .row("Elapsed", std::format("{:.3f} s", elapsed_sec))
                                 .row("Throughput", std::format("{:.0f} ops/s", throughput))
                                 .row("ns per op", std::format("{:.2f} ns", 1e9 / throughput))
                                 .indent_size(1)
                                 .value_align(demiplane::ink::Align::Right)
                                 .render();

    std::cout << '\n'
              << demiplane::ink::box(body)
                     .title("Disruptor — 4P→1C (batched 16)")
                     .border(demiplane::ink::border::unicode)
                     .border_style(demiplane::ink::colors::bold_magenta)
                     .terminate()
                     .render();
}

/*==============================================================================
 * SPSC PURE THROUGHPUT (1P1C) — MultiProducerSequencer vs SingleProducerSequencer
 *
 * Single producer + single consumer, each pinned to a distinct physical core, both
 * running flat out. Templated on the sequencer so we run the SAME loop with each:
 *   - MultiProducerSequencer: every claim is a fetch-add + rotation-flag store; the
 *     consumer scans availability flags. Correct for N producers, overkill for one.
 *   - SingleProducerSequencer: no CAS, no availability buffer — the producer owns a
 *     plain claimed counter and the consumer reads the published cursor directly.
 * The consumer here tracks the producer near-lockstep (small batches), so this is the
 * latency-coupled regime; see the batching sweep below for the high-throughput regime.
 *============================================================================*/
template <template <typename> class SequencerT>
void spsc_throughput_run(const char* title) {
    constexpr std::int64_t BUFFER_SIZE   = 1 << 16;  // 65536 — large buffer minimises backpressure stalls
    constexpr std::int64_t TOTAL_ENTRIES = 100'000'000;
    // Distinct physical cores, deliberately not adjacent ids to avoid landing on
    // SMT siblings (which would share an execution core and skew the numbers).
    constexpr int PRODUCER_CORE          = 2;
    constexpr int CONSUMER_CORE          = 4;

    Disruptor<std::int64_t, SequencerT, BusySpinWaitStrategy> disruptor{BUFFER_SIZE};

    // All three threads (main + producer + consumer) rendezvous here so the timer
    // starts the instant both workers begin.
    std::barrier sync_point{3};

    // Sink for the consumer's checksum — a visible side effect so the optimiser
    // cannot elide the ring-buffer reads (which would make throughput meaningless).
    std::atomic<std::int64_t> sink;

    // SPSC with a known count: the consumer drains exactly TOTAL_ENTRIES and exits.
    std::thread consumer{[&]() {
        pin_current_thread_to_core(CONSUMER_CORE);
        sync_point.arrive_and_wait();

        std::int64_t next_seq  = 0;
        std::int64_t processed = 0;
        std::int64_t checksum  = 0;

        while (processed < TOTAL_ENTRIES) {
            const std::int64_t cursor = disruptor.sequencer().get_cursor();

            if (const std::int64_t available = disruptor.sequencer().get_highest_published(next_seq, cursor);
                available >= next_seq) {
                for (std::int64_t seq = next_seq; seq <= available; ++seq) {
                    checksum += disruptor.ring_buffer()[seq];
                }

                processed += (available - next_seq + 1);
                next_seq   = available + 1;
                disruptor.sequencer().update_gating_sequence(available);
            }
        }
        sink.store(checksum, std::memory_order_relaxed);
    }};

    std::thread producer{[&]() {
        pin_current_thread_to_core(PRODUCER_CORE);
        sync_point.arrive_and_wait();

        for (std::int64_t i = 0; i < TOTAL_ENTRIES; ++i) {
            const std::int64_t seq       = disruptor.sequencer().next();
            disruptor.ring_buffer()[seq] = i;
            disruptor.sequencer().publish(seq);
        }
    }};

    sync_point.arrive_and_wait();  // release both workers together
    const auto start_time = std::chrono::steady_clock::now();

    producer.join();
    consumer.join();

    const auto elapsed       = std::chrono::steady_clock::now() - start_time;
    const double elapsed_sec = std::chrono::duration<double>(elapsed).count();
    const double throughput  = TOTAL_ENTRIES / elapsed_sec;

    const std::string body = demiplane::ink::section("")
                                 .row("Producers", 1)
                                 .row("Consumers", 1)
                                 .row("Producer core", PRODUCER_CORE)
                                 .row("Consumer core", CONSUMER_CORE)
                                 .row("Total entries", TOTAL_ENTRIES)
                                 .row("Buffer size", BUFFER_SIZE)
                                 .row("Elapsed", std::format("{:.3f} s", elapsed_sec))
                                 .row("Throughput", std::format("{:.0f} ops/s", throughput))
                                 .row("ns per op", std::format("{:.2f} ns", 1e9 / throughput))
                                 .indent_size(1)
                                 .value_align(demiplane::ink::Align::Right)
                                 .render();

    std::cout << '\n'
              << demiplane::ink::box(body)
                     .title(title)
                     .border(demiplane::ink::border::unicode)
                     .border_style(demiplane::ink::colors::bold_green)
                     .terminate()
                     .render();
}

/*==============================================================================
 * SPSC DIAGNOSTICS — publish rate + consumer batching sweep
 *
 * These explain the SPSC throughput regimes of our SingleProducerSequencer:
 *   - publish rate: producer flat-out into a buffer larger than the publish count
 *     (no wrap, no gating), with a light observer reading the cursor so the atomic
 *     publishes are not dead-code-eliminated. Isolates the sequencer's next()+publish().
 *   - batching sweep: end-to-end 1P1C where the consumer deliberately lets the producer
 *     get ahead (spin-pause without touching the cursor) so it drains large batches,
 *     amortizing the cross-core cursor handoff. Shows throughput vs. consumer batch size.
 *============================================================================*/
namespace {
    constexpr std::int64_t SPSC_BUFFER_SIZE   = 1 << 16;
    constexpr std::int64_t SPSC_TOTAL_ENTRIES = 100'000'000;
    constexpr int SPSC_PRODUCER_CORE          = 2;
    constexpr int SPSC_CONSUMER_CORE          = 4;

    constexpr std::int64_t PUBLISH_RATE_BUFFER  = 1 << 24;  // 16.7M slots > publish count ⇒ never wraps
    constexpr std::int64_t PUBLISH_RATE_ENTRIES = 1 << 23;  // 8.4M publishes

    void spsc_publish_rate_run() {
        Disruptor<std::int64_t, SingleProducerSequencer, BusySpinWaitStrategy> disruptor{PUBLISH_RATE_BUFFER};
        std::atomic done{false};
        std::atomic<std::int64_t> sink{0};
        // Observer reads the cursor so the producer's atomic publishes stay observable
        // (otherwise a local producer-only loop is dead-code-eliminated). The buffer is
        // larger than the publish count, so the producer never gates — this is publish rate.
        std::thread observer{[&]() {
            pin_current_thread_to_core(SPSC_CONSUMER_CORE);
            std::int64_t last = -1;
            while (!done.load(std::memory_order_relaxed)) {
                last = disruptor.sequencer().get_cursor();
            }
            sink.store(last, std::memory_order_relaxed);
        }};
        pin_current_thread_to_core(SPSC_PRODUCER_CORE);
        const auto start = std::chrono::steady_clock::now();
        for (std::int64_t i = 0; i < PUBLISH_RATE_ENTRIES; ++i) {
            const std::int64_t seq       = disruptor.sequencer().next();
            disruptor.ring_buffer()[seq] = i;
            disruptor.sequencer().publish(seq);
        }
        const double sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        done.store(true, std::memory_order_relaxed);
        observer.join();
        std::printf("  publish-rate: SingleProducerSequencer = %.0f ops/s  (%.2f ns/op)\n",
                    static_cast<double>(PUBLISH_RATE_ENTRIES) / sec,
                    1e9 / (static_cast<double>(PUBLISH_RATE_ENTRIES) / sec));
    }

    void spsc_batching_sweep_run(const int spin_between) {
        Disruptor<std::int64_t, SingleProducerSequencer, BusySpinWaitStrategy> disruptor{SPSC_BUFFER_SIZE};
        std::barrier sync_point{3};
        std::atomic<std::int64_t> sink{0};
        std::atomic<std::int64_t> batches_out{0};

        std::thread consumer{[&]() {
            pin_current_thread_to_core(SPSC_CONSUMER_CORE);
            sync_point.arrive_and_wait();
            std::int64_t next_seq  = 0;
            std::int64_t processed = 0;
            std::int64_t checksum  = 0;
            std::int64_t batches   = 0;
            while (processed < SPSC_TOTAL_ENTRIES) {
                const std::int64_t cursor = disruptor.sequencer().get_cursor();
                if (const std::int64_t available = disruptor.sequencer().get_highest_published(next_seq, cursor);
                    available >= next_seq) {
                    for (std::int64_t seq = next_seq; seq <= available; ++seq) {
                        checksum += disruptor.ring_buffer()[seq];
                    }
                    processed += available - next_seq + 1;
                    next_seq   = available + 1;
                    ++batches;
                    disruptor.sequencer().update_gating_sequence(available);
                    // Let the producer build the next batch — no cursor reads here.
                    for (int s = 0; s < spin_between; ++s) {
                        pause_arc_agnostic();
                    }
                } else {
                    pause_arc_agnostic();
                }
            }
            sink.store(checksum, std::memory_order_relaxed);
            batches_out.store(batches, std::memory_order_relaxed);
        }};
        std::thread producer{[&]() {
            pin_current_thread_to_core(SPSC_PRODUCER_CORE);
            sync_point.arrive_and_wait();
            for (std::int64_t i = 0; i < SPSC_TOTAL_ENTRIES; ++i) {
                const std::int64_t seq       = disruptor.sequencer().next();
                disruptor.ring_buffer()[seq] = i;
                disruptor.sequencer().publish(seq);
            }
        }};

        sync_point.arrive_and_wait();
        const auto start = std::chrono::steady_clock::now();
        producer.join();
        consumer.join();
        const double sec  = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        const double avg  = static_cast<double>(SPSC_TOTAL_ENTRIES) / static_cast<double>(batches_out.load());
        const double tput = static_cast<double>(SPSC_TOTAL_ENTRIES) / sec;
        std::printf("  spin_between=%-5d  avg_batch=%8.1f  throughput=%.0f ops/s  (%.2f ns/op)\n",
                    spin_between,
                    avg,
                    tput,
                    1e9 / tput);
    }

    void print_group_header(std::string_view title) {
        std::cout << '\n'
                  << demiplane::ink::colors::colorize(demiplane::ink::colors::bold_yellow, std::string{title}) << '\n';
    }
}  // namespace

int main() {
    std::cout << demiplane::ink::box("Disruptor Performance Benchmarks")
                     .border(demiplane::ink::border::unicode)
                     .border_style(demiplane::ink::colors::bold_cyan)
                     .terminate()
                     .render();

    print_group_header("  MULTI-PRODUCER THROUGHPUT (N producers → 1 consumer)");
    mpsc_one_at_a_time_test();
    mpsc_batched_test();

    print_group_header("  SPSC THROUGHPUT (pinned, 1P1C)");
    spsc_throughput_run<MultiProducerSequencer>("SPSC 1P1C — MultiProducerSequencer");
    spsc_throughput_run<SingleProducerSequencer>("SPSC 1P1C — SingleProducerSequencer");

    print_group_header("  SPSC PRODUCER PUBLISH RATE (no consumer, no wrap — sequencer only)");
    spsc_publish_rate_run();

    print_group_header("  SPSC CONSUMER BATCHING SWEEP (let the producer get ahead)");
    for (const int spin : {0, 64, 256, 1024, 4096, 16384}) {
        spsc_batching_sweep_run(spin);
    }

    std::cout << '\n'
              << demiplane::ink::box("Benchmarks Complete!")
                     .border(demiplane::ink::border::unicode)
                     .border_style(demiplane::ink::colors::bold_green)
                     .terminate()
                     .render();

    return 0;
}
