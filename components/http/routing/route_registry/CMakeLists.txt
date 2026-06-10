##############################################################################
# Http Routing — RouteRegistry (exact + parametric route table)
##############################################################################
add_library(${DMP_HTTP}.Routing.RouteRegistry STATIC route_registry.cpp)

target_include_directories(${DMP_HTTP}.Routing.RouteRegistry PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${DMP_HTTP}.Routing.RouteRegistry
        PUBLIC
        ${DMP_HTTP}.Routing.Middleware
        ${DMP_HTTP}.Types.Enums
        ${DMP_HTTP}.Types.Errors
        Demiplane::Common::Gears
        Boost::container
        Boost::unordered
        PRIVATE
        ${DMP_HTTP}.Types.UrlDecode
)
##############################################################################
