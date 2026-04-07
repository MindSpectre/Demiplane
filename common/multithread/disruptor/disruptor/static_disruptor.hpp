#pragma once

#include <gears_class_traits.hpp>

#include "multi_producer_sequencer/static_multi_producer_sequencer.hpp"
#include "ring_buffer/static_ring_buffer.hpp"

namespace demiplane::multithread {
    // All components available via this single include
    template <typename T, std::size_t BUFFER_SIZE>
    class StaticDisruptor : gears::Immutable {
    public:
        constexpr explicit StaticDisruptor(std::unique_ptr<WaitStrategy> wait_strategy,
                                           std::int64_t initial_cursor = -1)
            : sequencer_{std::move(wait_strategy), initial_cursor} {
        }
        ~StaticDisruptor() = default;

        [[nodiscard]] StaticMultiProducerSequencer<BUFFER_SIZE>& sequencer() noexcept {
            return sequencer_;
        }
        [[nodiscard]] const StaticMultiProducerSequencer<BUFFER_SIZE>& sequencer() const noexcept {
            return sequencer_;
        }

        [[nodiscard]] StaticRingBuffer<T, BUFFER_SIZE>& ring_buffer() noexcept {
            return ring_buffer_;
        }
        [[nodiscard]] const StaticRingBuffer<T, BUFFER_SIZE>& ring_buffer() const noexcept {
            return ring_buffer_;
        }

    private:
        StaticMultiProducerSequencer<BUFFER_SIZE> sequencer_;
        StaticRingBuffer<T, BUFFER_SIZE> ring_buffer_;
    };
}  // namespace demiplane::multithread
