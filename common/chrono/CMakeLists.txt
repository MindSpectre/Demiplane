set(DMP_CHRONO ${DMP_COMMON}.Chrono)

##############################################################################
# Utilities for chrono tools
##############################################################################
add_library(${DMP_CHRONO}.Utilities INTERFACE
        utilities/chrono_utils.hpp
)
target_include_directories(${DMP_CHRONO}.Utilities
        INTERFACE
        utilities/
)
target_link_libraries(${DMP_CHRONO}.Utilities INTERFACE
        Demiplane::Common::Gears
        Boost::asio
)
##############################################################################

##############################################################################
# Stopwatch class with measure purposes
##############################################################################
add_library(${DMP_CHRONO}.Stopwatch INTERFACE
        stopwatch/include/stopwatch.hpp
        stopwatch/include/printing_stopwatch.hpp
)
target_include_directories(${DMP_CHRONO}.Stopwatch
        INTERFACE
        stopwatch/include
)
target_link_libraries(${DMP_CHRONO}.Stopwatch
        INTERFACE
        Demiplane::Common::Gears
)
##############################################################################

##############################################################################
# Clock for checking time
##############################################################################
add_library(${DMP_CHRONO}.Clock INTERFACE
        clock/clock.hpp
)
target_include_directories(${DMP_CHRONO}.Clock
        INTERFACE
        clock/
)
##############################################################################

##############################################################################
# Timer for managing function flow
##############################################################################
add_library(${DMP_CHRONO}.Timer STATIC
        timer/include/timer.hpp
        timer/source/timer.inl
        timer/source/timer.cpp
)
target_include_directories(${DMP_CHRONO}.Timer
        PUBLIC
        timer/include
)
target_link_libraries(${DMP_CHRONO}.Timer PUBLIC
        Demiplane::Common::Gears
        Demiplane::Common::Multithread
)
##############################################################################

##############################################################################
# Chrono exported library
##############################################################################
add_combined_library(${DMP_CHRONO}
        SOURCES
        export/demiplane/chrono
        DIRECTORIES
        export
        LIBRARIES
        ${DMP_CHRONO}.Clock
        ${DMP_CHRONO}.Stopwatch
        ${DMP_CHRONO}.Utilities
        ${DMP_CHRONO}.Timer
)

add_library(Demiplane::Common::Chrono ALIAS ${DMP_CHRONO})
##############################################################################
