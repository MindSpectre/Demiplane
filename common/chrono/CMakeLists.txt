set(DMP_CHRONO ${DMP_COMMON}.Chrono)

##############################################################################
# Stopwatch class with measure purposes
##############################################################################
add_library(${DMP_CHRONO}.Stopwatch STATIC
        stopwatch/source/stopwatch.cpp
        stopwatch/include/lw_stopwatch.hpp
        stopwatch/include/testing_stopwatch.hpp
)
target_include_directories(${DMP_CHRONO}.Stopwatch
        PUBLIC
        stopwatch/include
)
target_link_libraries(${DMP_CHRONO}.Stopwatch
        PUBLIC
        Demiplane::Common::Gears
)
##############################################################################

##############################################################################
# Stopwatch class with measure purposes
##############################################################################
add_library(${DMP_CHRONO}.Clock INTERFACE
)
target_include_directories(${DMP_CHRONO}.Clock
        INTERFACE
        clock/
)
##############################################################################

##############################################################################
# Chrono exported library
##############################################################################
add_combined_library(${DMP_CHRONO}
        DIRECTORIES
        export
        LIBRARIES
        ${DMP_CHRONO}.Clock
        ${DMP_CHRONO}.Stopwatch
)

add_library(Demiplane::Common::Chrono ALIAS ${DMP_CHRONO})
##############################################################################
