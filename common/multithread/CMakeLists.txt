set(DMP_MULTITHREAD ${DMP_COMMON}.Multithread)

##############################################################################
# Synchronized resource class - wraps resource with mutex
##############################################################################
add_library(${DMP_MULTITHREAD}.SyncResource INTERFACE

)
target_include_directories(${DMP_MULTITHREAD}.SyncResource
        INTERFACE
        sync_resource/include/
)
##############################################################################


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

target_link_libraries(${DMP_MULTITHREAD}.ThreadPool PUBLIC
        Demiplane::Common::Gears
        ${DMP_MULTITHREAD}.SyncResource
)
##############################################################################




##############################################################################
# Multithread exported library
##############################################################################
add_combined_library(${DMP_MULTITHREAD}
        DIRECTORIES
        export
        LIBRARIES
        ${DMP_MULTITHREAD}.ThreadPool
        ${DMP_MULTITHREAD}.SyncResource
)

add_library(Demiplane::Common::Multithread ALIAS ${DMP_MULTITHREAD})
##############################################################################
