##############################################################################
#
##############################################################################
#add_library(${DMP_DATABASE_POSTGRESQL}.Capabilities.AsyncExecutor STATIC
#        async_executor/source/postgres_async_executor.cpp
#        async_executor/include/postgres_async_executor.hpp
#)
#
#target_include_directories(${DMP_DATABASE_POSTGRESQL}.Capabilities.AsyncExecutor PUBLIC
#        async_executor/include/
#)
#
#target_link_libraries(${DMP_DATABASE_POSTGRESQL}.Capabilities.AsyncExecutor PUBLIC
#        ${DMP_DATABASE}.Base.Errors
#        ${DMP_DATABASE_POSTGRESQL}.Result
#        ${DMP_DATABASE_POSTGRESQL}.Params
#        ${DMP_DATABASE}.Query.Compiler
#        Demiplane::Common::Gears
#        Boost::asio
#)
##############################################################################

##############################################################################
# Sync Executor
##############################################################################
add_library(${DMP_DATABASE_POSTGRESQL}.Capabilities.SyncExecutor STATIC
        sync_executor/source/postgres_sync_executor.cpp
        sync_executor/include/postgres_sync_executor.hpp
)

target_include_directories(${DMP_DATABASE_POSTGRESQL}.Capabilities.SyncExecutor PUBLIC
        sync_executor/include/
)

target_link_libraries(${DMP_DATABASE_POSTGRESQL}.Capabilities.SyncExecutor PUBLIC
        ${DMP_DATABASE}.Base.Errors
        ${DMP_DATABASE_POSTGRESQL}.Result
        ${DMP_DATABASE_POSTGRESQL}.Params
        ${DMP_DATABASE_POSTGRESQL}.Errors
        ${DMP_DATABASE}.Query.Compiler
        Demiplane::Common::Gears
)
##############################################################################