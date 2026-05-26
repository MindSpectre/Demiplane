#pragma once

#include <concepts>
#include <cstdint>

namespace demiplane::multithread {

    /**
     * @brief Compile-time contract shared by every sequencer (the claim/publish/consume
     *        coordinator behind a Disruptor).
     *
     * Both MultiProducerSequencer and SingleProducerSequencer satisfy this, so either can
     * be plugged into `Disruptor<T, SequencerT, WaitStrategyT>` and the producer/consumer
     * call sites stay identical. The method surface mirrors the existing sequencer API:
     *
     *   producer : next(), next_batch(n), try_next(), publish(seq), publish_batch(lo, hi)
     *   consumer : get_highest_published(lo, hi), is_available(seq), update_gating_sequence(seq),
     *              wait_for(seq), signal_all()
     *   query    : get_cursor(), get_gating_sequence(), remaining_capacity()
     *
     * Note: there is intentionally no `constructible_from` clause. Construction is checked
     * where it happens — at the `Disruptor` constructor's in-place sequencer init — which
     * gives a clear per-call-site error and avoids wrongly rejecting wait strategies that
     * are not default-constructible (e.g. AnyWaitStrategy).
     * `signal()` is also intentionally absent: it is only ever called internally by the
     * sequencer's own publish(), never externally.
     */
    template <typename S>
    concept IsSequencer = requires(
        S s, const S cs, const std::int64_t seq, const std::int64_t n, const std::int64_t lo, const std::int64_t hi) {
        { s.next() } -> std::convertible_to<std::int64_t>;
        { s.next_batch(n) } -> std::convertible_to<std::int64_t>;
        { s.try_next() } -> std::convertible_to<std::int64_t>;
        { s.publish(seq) };
        { s.publish_batch(lo, hi) };
        { s.update_gating_sequence(seq) };
        { s.wait_for(seq) } -> std::convertible_to<std::int64_t>;
        { s.signal_all() };
        { cs.get_highest_published(lo, hi) } -> std::convertible_to<std::int64_t>;
        { cs.is_available(seq) } -> std::convertible_to<bool>;
        { cs.get_cursor() } -> std::convertible_to<std::int64_t>;
        { cs.get_gating_sequence() } -> std::convertible_to<std::int64_t>;
        { cs.remaining_capacity() } -> std::convertible_to<std::int64_t>;
    };

}  // namespace demiplane::multithread
