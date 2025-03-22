##############################################################################
# Stopwatch class with measure purposes
##############################################################################
add_library(${PROJECT_NAME}_Stopwatch STATIC
        source/stopwatch.cpp
        include/stopwatch.hpp
)
target_include_directories(${PROJECT_NAME}_Stopwatch
        PUBLIC
        include/
)
target_link_libraries(${PROJECT_NAME}_Stopwatch
        PUBLIC
        Demiplane::Scroll
)
add_library(Demiplane::Stopwatch ALIAS ${PROJECT_NAME}_Stopwatch)
##############################################################################