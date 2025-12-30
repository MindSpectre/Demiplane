add_library(${DMP_DATABASE_POSTGRESQL}.Capabilities.Executor.Interface INTERFACE
    interface/postgres_executor.hpp
)


target_include_directories(${DMP_DATABASE_POSTGRESQL}.Capabilities.Executor.Interface INTERFACE
        interface
)

target_link_libraries(${DMP_DATABASE_POSTGRESQL}.Capabilities.Executor.Interface INTERFACE
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
        async_executor/source/postgres_async_executor.cpp
        async_executor/include/postgres_async_executor.hpp
)

target_include_directories(${DMP_DATABASE_POSTGRESQL}.Capabilities.Executor.Async PUBLIC
        async_executor/include/
)

target_link_libraries(${DMP_DATABASE_POSTGRESQL}.Capabilities.Executor.Async PUBLIC
        ${DMP_DATABASE_POSTGRESQL}.Capabilities.Executor.Interface
        Boost::asio
)
##############################################################################

##############################################################################
# Sync Executor
##############################################################################
add_library(${DMP_DATABASE_POSTGRESQL}.Capabilities.Executor.Sync STATIC
        sync_executor/source/postgres_sync_executor.cpp
        sync_executor/include/postgres_sync_executor.hpp
)

target_include_directories(${DMP_DATABASE_POSTGRESQL}.Capabilities.Executor.Sync PUBLIC
        sync_executor/include/
)

target_link_libraries(${DMP_DATABASE_POSTGRESQL}.Capabilities.Executor.Sync PUBLIC
        ${DMP_DATABASE_POSTGRESQL}.Capabilities.Executor.Interface
)
##############################################################################