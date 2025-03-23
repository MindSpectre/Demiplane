set(MULTITHREAD Multithread)

##############################################################################
# Thread pool for paralleling tasks
##############################################################################
add_library(${PROJECT_NAME}.${MULTITHREAD}.ThreadPool
        thread_pool/source/thread_pool.cpp
        thread_pool/include/thread_pool.hpp
)
target_include_directories(${PROJECT_NAME}.${MULTITHREAD}.ThreadPool
        PUBLIC
        thread_pool/include/
)
add_library(Demiplane::Multithread::ThreadPool ALIAS ${PROJECT_NAME}.${MULTITHREAD}.ThreadPool)
##############################################################################