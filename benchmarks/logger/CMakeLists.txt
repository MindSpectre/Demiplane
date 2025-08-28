##############################################################################
#
##############################################################################
add_executable(${DMP_BENCHMARKS}.Scroll.Logger
        file_logger.cpp
)
target_link_libraries(${DMP_BENCHMARKS}.Scroll.Logger
        PRIVATE
        Demiplane::Common::Scroll
        Demiplane::Common::Nexus
        Demiplane::Common::Math
)

##############################################################################