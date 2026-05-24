set(DMP_MULTITHREAD ${DMP_COMMON}.Multithread)

##############################################################################
# Multithread utils
##############################################################################
add_library(${DMP_MULTITHREAD}.Utils INTERFACE
        utils/pause.hpp
        utils/thread_pinning.hpp
)
target_include_directories(${DMP_MULTITHREAD}.Utils
        INTERFACE
        utils
)
target_link_libraries(${DMP_MULTITHREAD}.Utils
        INTERFACE
        Threads::Threads
)
##############################################################################


##############################################################################
# EventCount - folly-style futex wait primitive
##############################################################################
add_library(${DMP_MULTITHREAD}.EventCount INTERFACE
        event_count/event_count.hpp
)
target_include_directories(${DMP_MULTITHREAD}.EventCount
        INTERFACE
        event_count
)
target_link_libraries(${DMP_MULTITHREAD}.EventCount
        INTERFACE
        Threads::Threads
)
##############################################################################


##############################################################################
# ResourcePool - partition-aware bounded resource pool
##############################################################################
add_library(${DMP_MULTITHREAD}.ResourcePool.Sync INTERFACE
        resource_pool/sync/resource_pool.hpp
        resource_pool/sync/lease.hpp
)
target_include_directories(${DMP_MULTITHREAD}.ResourcePool.Sync
        INTERFACE
        resource_pool/sync
)
target_link_libraries(${DMP_MULTITHREAD}.ResourcePool.Sync
        INTERFACE
        Threads::Threads
        Demiplane::Common::Gears
        ${DMP_MULTITHREAD}.EventCount
        ${DMP_MULTITHREAD}.Utils
)
##############################################################################

##############################################################################
# ResourcePool - partition-aware bounded resource pool
##############################################################################
add_library(${DMP_MULTITHREAD}.ResourcePool.Async INTERFACE
        resource_pool/async/async_resource_pool.hpp
        resource_pool/async/async_lease.hpp
)
target_include_directories(${DMP_MULTITHREAD}.ResourcePool.Async
        INTERFACE
        resource_pool/async
)
target_link_libraries(${DMP_MULTITHREAD}.ResourcePool.Async
        INTERFACE
        Threads::Threads
        Demiplane::Common::Gears
        ${DMP_MULTITHREAD}.EventCount
        ${DMP_MULTITHREAD}.Utils
)
##############################################################################

##############################################################################
# Synchronized resource class - wraps resource with mutex
##############################################################################
add_library(${DMP_MULTITHREAD}.ThreadSafeResource INTERFACE
        thread_safe_resource/include/thread_safe_resource.hpp
)
target_include_directories(${DMP_MULTITHREAD}.ThreadSafeResource
        INTERFACE
        thread_safe_resource/include/
)
target_link_libraries(${DMP_MULTITHREAD}.ThreadSafeResource
        INTERFACE
        Threads::Threads
)
##############################################################################


##############################################################################
# Thread pool for paralleling tasks
##############################################################################
add_library(${DMP_MULTITHREAD}.ThreadPool STATIC
        thread_pool/source/thread_pool.cpp
        thread_pool/include/thread_pool.hpp
        thread_pool/enqueued_task.hpp
        thread_pool/include/thread_pool_config.hpp
)
target_include_directories(${DMP_MULTITHREAD}.ThreadPool
        PUBLIC
        thread_pool/include/
)

target_link_libraries(${DMP_MULTITHREAD}.ThreadPool PUBLIC
        Demiplane::Common::Gears
        ${DMP_MULTITHREAD}.ThreadSafeResource
)
##############################################################################


##############################################################################
# Disruptor - Lock-free ring buffer for high-performance messaging
##############################################################################
add_subdirectory(disruptor)
##############################################################################


##############################################################################
# Multithread exported library
##############################################################################
add_combined_library(${DMP_MULTITHREAD}
        SOURCES
        export/demiplane/multithread
        DIRECTORIES
        export
        LIBRARIES
        ${DMP_MULTITHREAD}.ThreadPool
        ${DMP_MULTITHREAD}.ThreadSafeResource
        ${DMP_MULTITHREAD}.Disruptor
        ${DMP_MULTITHREAD}.EventCount
        ${DMP_MULTITHREAD}.ResourcePool.Sync
        ${DMP_MULTITHREAD}.ResourcePool.Async
)

add_library(Demiplane::Common::Multithread ALIAS ${DMP_MULTITHREAD})
##############################################################################
