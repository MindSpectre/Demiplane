##############################################################################
# Stopwatch class with measure purposes
##############################################################################
add_library(${PROJECT_NAME}_Stopwatch
        source/stopwatch.cpp
        include/stopwatch.hpp
)
target_include_directories(${PROJECT_NAME}_Stopwatch
        PUBLIC
        include/
)
add_library(Demiplane::Stopwatch ALIAS ${PROJECT_NAME}_Stopwatch)
##############################################################################