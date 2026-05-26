#pragma once

#include <cstddef>
#include <utility>

#include <gears_class_traits.hpp>

#include "ring_buffer/ring_buffer.hpp"
#include "ring_buffer/static_ring_buffer.hpp"
#include "sequencer/multi_producer_sequencer.hpp"
#include "sequencer/sequencer_concept.hpp"
#include "sequencer/single_producer_sequencer.hpp"
#include "wait_strategies/blocking.hpp"
#include "wait_strategies/busy_spin.hpp"
#include "wait_strategies/timeout_blocking.hpp"
#include "wait_strategies/wait_strategy.hpp"
#include "wait_strategies/yielding_strategy.hpp"

/**
 * @file disruptor.hpp
 * @brief High-performance lock-free ring-buffer messaging (LMAX Disruptor pattern).
 *
 * `Disruptor<T, WS>` bundles a runtime-sized ring buffer with a multi-producer
 * sequencer. The single buffer allocation happens once at construction. The wait
 * strategy `WS` is a compile-time type stored by value, so there is no virtual
 * dispatch on the hot path (use `AnyWaitStrategy` to choose a strategy at runtime).
 *
 * Producer:
 * ```cpp
 *   Disruptor<LogEntry> d{1024};                 // default BusySpinWaitStrategy
 *   const std::int64_t seq = d.sequencer().next();   // lock-free fetch-add claim
 *   d.ring_buffer()[seq]   = make_entry();           // write the slot
 *   d.sequencer().publish(seq);                       // make available (release)
 * ```
 *
 * Consumer (single):
 * ```cpp
 *   std::int64_t next_seq = 0;
 *   for (;;) {
 *       const std::int64_t avail =
 *           d.sequencer().get_highest_published(next_seq, d.sequencer().get_cursor());
 *       for (std::int64_t seq = next_seq; seq <= avail; ++seq) {
 *           process(d.ring_buffer()[seq]);            // strict order, no gaps
 *       }
 *       next_seq = avail + 1;
 *       d.sequencer().update_gating_sequence(avail);  // backpressure
 *   }
 * ```
 *
 * There is no per-slot "mark consumed" step: availability is tracked by a rotation
 * number per slot (see MultiProducerSequencer), so the consumer never writes back.
 *
 * Choosing WS: BusySpin (lowest latency, 100% CPU), Yielding (balanced),
 * Blocking / TimeoutBlocking (low CPU). Buffer size must be a power of 2.
 */
namespace demiplane::multithread {

    template <typename T, template <typename> class SequencerT, IsWaitStrategy WaitStrategyT>
        requires IsSequencer<SequencerT<WaitStrategyT>>
    class Disruptor : gears::Immutable {
    public:
        /**
         * @param buffer_size Ring size; must be a non-zero power of 2.
         * @param ws_args     Forwarded to construct the wait strategy in place (e.g.
         *                    `Disruptor<T, MultiProducerSequencer, TimeoutBlockingWaitStrategy>{n, 100ms}`).
         */
        template <typename... WaitStrategyArgsT>
        explicit Disruptor(const std::size_t buffer_size, WaitStrategyArgsT&&... ws_args) noexcept
            : sequencer_{buffer_size, std::forward<WaitStrategyArgsT>(ws_args)...},
              ring_buffer_{buffer_size} {
        }

        [[nodiscard]] SequencerT<WaitStrategyT>& sequencer() noexcept {
            return sequencer_;
        }
        [[nodiscard]] const SequencerT<WaitStrategyT>& sequencer() const noexcept {
            return sequencer_;
        }

        [[nodiscard]] RingBuffer<T>& ring_buffer() noexcept {
            return ring_buffer_;
        }
        [[nodiscard]] const RingBuffer<T>& ring_buffer() const noexcept {
            return ring_buffer_;
        }

    private:
        SequencerT<WaitStrategyT> sequencer_;
        RingBuffer<T> ring_buffer_;
    };

}  // namespace demiplane::multithread
