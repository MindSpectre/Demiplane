##############################################################################
# Http Types — Response (stored allocator, fluent setters) — header-only
##############################################################################
add_library(${DMP_HTTP}.Types.Response INTERFACE response.hpp)

target_include_directories(${DMP_HTTP}.Types.Response INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${DMP_HTTP}.Types.Response INTERFACE
        ${DMP_HTTP}.Types.Body
        ${DMP_HTTP}.Types.Headers
        ${DMP_HTTP}.Types.Enums
)
##############################################################################
