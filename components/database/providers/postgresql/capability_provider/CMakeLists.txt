##############################################################################
# PostgreSql CapabilityProvider Concept (header-only)
##############################################################################
add_library(${DMP_DATABASE_POSTGRESQL}.CapabilityProvider INTERFACE
        capability_provider.hpp
)
target_include_directories(${DMP_DATABASE_POSTGRESQL}.CapabilityProvider INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(${DMP_DATABASE_POSTGRESQL}.CapabilityProvider INTERFACE
        ${DMP_DATABASE_POSTGRESQL}.Capabilities.Executor.Sync
        ${DMP_DATABASE_POSTGRESQL}.Capabilities.Executor.Async
        Boost::asio
)
##############################################################################
