##############################################################################
# Http Routing — GroupBinding (header-only)
##############################################################################
add_library(${DMP_HTTP}.Routing.Group INTERFACE group.hpp)

target_include_directories(${DMP_HTTP}.Routing.Group INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${DMP_HTTP}.Routing.Group INTERFACE
        ${DMP_HTTP}.Routing.Controller
        ${DMP_HTTP}.Routing.RouteRegistry
)
##############################################################################
