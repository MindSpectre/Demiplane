##############################################################################
# Http Types — protocol-agnostic enums (header-only)
##############################################################################
add_library(${DMP_HTTP}.Types.Enums INTERFACE http_enums.hpp)

target_include_directories(${DMP_HTTP}.Types.Enums INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${DMP_HTTP}.Types.Enums INTERFACE
        Boost::beast
)
##############################################################################
