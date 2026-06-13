##############################################################################
# Http Routing — Router (dispatch facade for drivers)
##############################################################################
add_library(${DMP_HTTP}.Routing.Router STATIC router.cpp)

target_include_directories(${DMP_HTTP}.Routing.Router PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${DMP_HTTP}.Routing.Router
        PUBLIC
        ${DMP_HTTP}.Routing.RouteRegistry
        Boost::asio
        PRIVATE
        ${DMP_HTTP}.Types.ErrorResponses
)
##############################################################################
