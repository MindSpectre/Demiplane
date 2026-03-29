add_library(${DMP_DATABASE_POSTGRESQL}.Capabilities.Executor.Shared INTERFACE
        shared/process_pgresult.hpp
        shared/executor_concept.hpp
)


target_include_directories(${DMP_DATABASE_POSTGRESQL}.Capabilities.Executor.Shared INTERFACE
        shared
)

target_link_libraries(${DMP_DATABASE_POSTGRESQL}.Capabilities.Executor.Shared INTERFACE
        ${DMP_DATABASE}.Query.Compiler
        ${DMP_DATABASE}.Base.Errors
        ${DMP_DATABASE_POSTGRESQL}.Errors
        ${DMP_DATABASE_POSTGRESQL}.Result
        ${DMP_DATABASE_POSTGRESQL}.Params
        Demiplane::Common::Gears
)

##############################################################################
# Async Executor
##############################################################################
add_library(${DMP_DATABASE_POSTGRESQL}.Capabilities.Executor.Async STATIC
        async_executor/postgres_async_executor.cpp
        async_executor/postgres_async_executor.hpp
)

target_include_directories(${DMP_DATABASE_POSTGRESQL}.Capabilities.Executor.Async PUBLIC
        async_executor/
)

target_link_libraries(${DMP_DATABASE_POSTGRESQL}.Capabilities.Executor.Async PUBLIC
        ${DMP_DATABASE_POSTGRESQL}.Capabilities.Executor.Shared
        ${DMP_DATABASE_POSTGRESQL}.ConnectionSlot
        Boost::asio
)
##############################################################################

##############################################################################
# Sync Executor
##############################################################################
add_library(${DMP_DATABASE_POSTGRESQL}.Capabilities.Executor.Sync STATIC
        sync_executor/postgres_sync_executor.cpp
        sync_executor/postgres_sync_executor.hpp
)

target_include_directories(${DMP_DATABASE_POSTGRESQL}.Capabilities.Executor.Sync PUBLIC
        sync_executor/
)

target_link_libraries(${DMP_DATABASE_POSTGRESQL}.Capabilities.Executor.Sync PUBLIC
        ${DMP_DATABASE_POSTGRESQL}.Capabilities.Executor.Shared
        ${DMP_DATABASE_POSTGRESQL}.ConnectionSlot
)
##############################################################################