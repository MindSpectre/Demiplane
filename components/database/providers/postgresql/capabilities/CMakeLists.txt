##############################################################################
# PostgreSql Connection
##############################################################################
#add_library(${DMP_DATABASE_POSTGRESQL}.Connection STATIC
#
#)
#target_include_directories(${DMP_DATABASE_POSTGRESQL}.Connection PUBLIC
#        include
#)
#target_link_libraries(${DMP_DATABASE_POSTGRESQL}.Connection
#        PUBLIC
#        ${DMP_DATABASE_POSTGRESQL}.Config
#        ${DMP_DATABASE_POSTGRESQL}.Result
#        ${DMP_DATABASE_POSTGRESQL}.Params
#        ${DMP_DATABASE_POSTGRESQL}.Shared
#        ${DMP_DATABASE}.Query.Compiler
#        Boost::asio
#)
##############################################################################

add_subdirectory(copier)

add_subdirectory(executors)

add_subdirectory(notifier)

add_subdirectory(pipeline)

add_library(${DMP_DATABASE_POSTGRESQL}.Capabilities INTERFACE)

target_link_libraries(${DMP_DATABASE_POSTGRESQL}.Capabilities INTERFACE
        ${DMP_DATABASE_POSTGRESQL}.Capabilities.Copier
        ${DMP_DATABASE_POSTGRESQL}.Capabilities.Notifier
        ${DMP_DATABASE_POSTGRESQL}.Capabilities.Executor.Async
        ${DMP_DATABASE_POSTGRESQL}.Capabilities.Executor.Sync
        ${DMP_DATABASE_POSTGRESQL}.Capabilities.Pipeline
)