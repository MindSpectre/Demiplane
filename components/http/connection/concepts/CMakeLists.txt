##############################################################################
# Http Connection — Connection / StreamConnection concepts (header-only)
##############################################################################
add_library(${DMP_HTTP}.Connection.Concepts INTERFACE
        connection_concepts.hpp
)

target_include_directories(${DMP_HTTP}.Connection.Concepts INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${DMP_HTTP}.Connection.Concepts INTERFACE
        ${DMP_HTTP}.Types.Enums
        ${DMP_HTTP}.Types.Executor
        Boost::asio
)
##############################################################################
