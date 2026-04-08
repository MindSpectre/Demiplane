#pragma once

#include <gears_class_traits.hpp>

#include "multi_producer_sequencer/dynamic_multi_producer_sequencer.hpp"
#include "ring_buffer/dynamic_ring_buffer.hpp"

namespace demiplane::multithread {

    template <typename T>
    class DynamicDisruptor : gears::Immutable {
    public:
        constexpr DynamicDisruptor(const std::size_t buffer_size,
                                   std::unique_ptr<WaitStrategy> wait_strategy,
                                   const std::int64_t initial_cursor = -1)
            : sequencer_{buffer_size, std::move(wait_strategy), initial_cursor},
              ring_buffer_{buffer_size} {
        }
        ~DynamicDisruptor() = default;

        [[nodiscard]] DynamicMultiProducerSequencer& sequencer() noexcept {
            return sequencer_;
        }
        [[nodiscard]] const DynamicMultiProducerSequencer& sequencer() const noexcept {
            return sequencer_;
        }

        [[nodiscard]] DynamicRingBuffer<T>& ring_buffer() noexcept {
            return ring_buffer_;
        }
        [[nodiscard]] const DynamicRingBuffer<T>& ring_buffer() const noexcept {
            return ring_buffer_;
        }

    private:
        DynamicMultiProducerSequencer sequencer_;
        DynamicRingBuffer<T> ring_buffer_;
    };
}  // namespace demiplane::multithread
