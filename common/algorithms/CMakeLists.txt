set(DMP_ALGORITHMS ${DMP_COMMON}.Algorithms)

add_library(${DMP_ALGORITHMS}.SlidingWindowSorter INTERFACE
        sliding_window_sorter/sliding_window_sorter.hpp
)
target_include_directories(${DMP_ALGORITHMS}.SlidingWindowSorter INTERFACE
        sliding_window_sorter/
)

add_combined_library(${DMP_ALGORITHMS}
        DIRECTORIES
        export
        SOURCES
        export/demiplane/algorithms
        LIBRARIES
        ${DMP_ALGORITHMS}.SlidingWindowSorter
)

add_library(Demiplane::Common::Algorithms ALIAS ${DMP_ALGORITHMS})