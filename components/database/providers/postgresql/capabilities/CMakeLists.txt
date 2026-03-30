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
#        ${DMP_DATABASE_POSTGRESQL}.Registries
#        ${DMP_DATABASE}.Query.Compiler
#        Boost::asio
#)
##############################################################################

add_subdirectory(copier)

add_subdirectory(executors)

add_subdirectory(notifier)

add_subdirectory(pipeline)

##############################################################################
# PostgreSql Capabilities + Provider Concept
##############################################################################
add_library(${DMP_DATABASE_POSTGRESQL}.Capabilities INTERFACE
        provider/capability_provider.hpp
)
target_include_directories(${DMP_DATABASE_POSTGRESQL}.Capabilities INTERFACE
        provider/
)
target_link_libraries(${DMP_DATABASE_POSTGRESQL}.Capabilities INTERFACE
        ${DMP_DATABASE_POSTGRESQL}.Capabilities.Copier
        ${DMP_DATABASE_POSTGRESQL}.Capabilities.Notifier
        ${DMP_DATABASE_POSTGRESQL}.Capabilities.Executor.Async
        ${DMP_DATABASE_POSTGRESQL}.Capabilities.Executor.Sync
        ${DMP_DATABASE_POSTGRESQL}.Capabilities.Pipeline
)
##############################################################################