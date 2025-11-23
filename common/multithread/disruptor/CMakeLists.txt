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
        include/disruptor.hpp
        include/multi_producer_sequencer.hpp
        include/ring_buffer.hpp
        include/sequence.hpp
        include/wait_strategies/wait_strategy.hpp
        include/wait_strategies/blocking.hpp
        include/wait_strategies/busy_spin.hpp
        include/wait_strategies/timeout_blocking.hpp
        include/wait_strategies/yielding_strategy.hpp
)

target_include_directories(${DMP_MULTITHREAD}.Disruptor INTERFACE
        include/
)

target_link_libraries(${DMP_MULTITHREAD}.Disruptor INTERFACE
        Threads::Threads
        Demiplane::Common::Gears
)
##############################################################################
