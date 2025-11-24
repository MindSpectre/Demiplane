##############################################################################
# Benchmark Disruptor
##############################################################################
add_executable(${DMP_BENCHMARKS}.Multithread.Disruptor
        disruptor_speed_test.cpp
)
target_link_libraries(${DMP_BENCHMARKS}.Multithread.Disruptor
        PRIVATE
        Demiplane::Common::Multithread
)
add_compile_options(${DMP_BENCHMARK}.Multithread.Disruptor
    -O3
)
##############################################################################