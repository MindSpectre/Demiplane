##############################################################################
# Disruptor - High-performance lock-free ring buffer implementation
##############################################################################
# The Disruptor pattern provides:
# - Lock-free multi-producer, single-consumer messaging
# - Strong ordering guarantees across threads
# - Sub-microsecond latency
# - Cache-friendly design with false-sharing prevention
#
# Components:
# - Sequence: Cache-aligned atomic sequence counters
# - RingBuffer: Power-of-2 circular buffer with O(1) indexing
# - WaitStrategy: Configurable consumer wait strategies
# - MultiProducerSequencer: Claim/publish protocol coordinator
##############################################################################
add_library(${DMP_MULTITHREAD}.Disruptor INTERFACE
        wait_strategies/blocking.hpp
        wait_strategies/busy_spin.hpp
        wait_strategies/timeout_blocking.hpp
        wait_strategies/yielding_strategy.hpp
        wait_strategies/wait_strategy.hpp
        shared/constants.hpp
        sequencer/sequencer_concept.hpp
        sequencer/multi_producer_sequencer.hpp
        sequencer/single_producer_sequencer.hpp
        sequence.hpp
        ring_buffer/ring_buffer.hpp
        ring_buffer/static_ring_buffer.hpp
        disruptor.hpp
)

target_include_directories(${DMP_MULTITHREAD}.Disruptor INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${DMP_MULTITHREAD}.Disruptor INTERFACE
        Threads::Threads
        Demiplane::Common::Gears
        ${DMP_MULTITHREAD}.Utils
)
##############################################################################
