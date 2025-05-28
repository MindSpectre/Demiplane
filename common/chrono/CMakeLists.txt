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
        Demiplane::Scroll
        Demiplane::Gears
)
##############################################################################

##############################################################################
# Stopwatch class with measure purposes
##############################################################################
#add_library(${DMP_CHRONO}.Timer INTERFACE
#        stopwatch/include/stopwatch.hpp
#)
#target_include_directories(${DMP_CHRONO}.Timer
#        INTERFACE
#        stopwatch/
#)
#target_link_libraries(${DMP_CHRONO}.Timer
#        INTERFACE
#        Demiplane::Scroll
#)
#add_library(Demiplane::Chrono::Timer ALIAS ${DMP_CHRONO}.Timer)
##############################################################################

##############################################################################
# Stopwatch class with measure purposes
##############################################################################
add_library(${DMP_CHRONO}.Utilities INTERFACE
)
target_include_directories(${DMP_CHRONO}.Utilities
        INTERFACE
        utilities/
)
##############################################################################

add_combined_library(${DMP_CHRONO}
        DIRECTORIES
        export
        LIBRARIES
        ${DMP_CHRONO}.Utilities
        ${DMP_CHRONO}.Stopwatch
        ALIAS
        Demiplane::Chrono
)