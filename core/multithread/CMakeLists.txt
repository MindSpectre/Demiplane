set(DMP_MULTITHREAD ${DMP_CORE}.Multithread)

##############################################################################
# Thread pool for paralleling tasks
##############################################################################
add_library(${DMP_MULTITHREAD}.ThreadPool STATIC
        thread_pool/source/thread_pool.cpp
        thread_pool/include/thread_pool.hpp
        thread_pool/enqueued_task.hpp
)
target_include_directories(${DMP_MULTITHREAD}.ThreadPool
        PUBLIC
        thread_pool/include/
)
##############################################################################

add_combined_library(${DMP_MULTITHREAD}
        DIRECTORIES
        export
        LIBRARIES
        ${DMP_MULTITHREAD}.ThreadPool
        ALIAS
        Demiplane::Multithread
)
