##############################################################################
# Benchmark File logger
##############################################################################
add_executable(${DMP_BENCHMARKS}.Scroll.FileLogger
        file_logger.cpp
)
target_link_libraries(${DMP_BENCHMARKS}.Scroll.FileLogger
        PRIVATE
        Demiplane::Common::Scroll
        Demiplane::Common::Nexus
        Demiplane::Common::Math
)

##############################################################################