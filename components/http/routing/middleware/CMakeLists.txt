##############################################################################
# Http Routing — middleware shape (header-only)
##############################################################################
add_library(${DMP_HTTP}.Routing.Middleware INTERFACE middleware.hpp)

target_include_directories(${DMP_HTTP}.Routing.Middleware INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${DMP_HTTP}.Routing.Middleware INTERFACE
        ${DMP_HTTP}.Types.AsyncOutcome
        ${DMP_HTTP}.Types.RequestContext
)
##############################################################################
