##############################################################################
# Http Types — Request (string_view target, value Body) — header-only
##############################################################################
add_library(${DMP_HTTP}.Types.Request INTERFACE request.hpp)

target_include_directories(${DMP_HTTP}.Types.Request INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${DMP_HTTP}.Types.Request INTERFACE
        ${DMP_HTTP}.Types.Body
        ${DMP_HTTP}.Types.Headers
        ${DMP_HTTP}.Types.Enums
)
##############################################################################
