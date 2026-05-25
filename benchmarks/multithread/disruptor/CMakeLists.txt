##############################################################################
# Benchmark Disruptor
##############################################################################
add_executable(${DMP_BENCHMARKS}.Multithread.Disruptor
        disruptor_speed_test.cpp
)
target_link_libraries(${DMP_BENCHMARKS}.Multithread.Disruptor
        PRIVATE
        Demiplane::Common::Multithread
        Demiplane::Common::Ink
        Threads::Threads
)
target_compile_options(${DMP_BENCHMARKS}.Multithread.Disruptor PRIVATE -O3)
##############################################################################