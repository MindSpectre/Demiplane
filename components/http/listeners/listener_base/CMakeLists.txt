##############################################################################
# Http Listeners — ListenerBase (abstract interface; header-only)
##############################################################################
add_library(${DMP_HTTP}.Listeners.Base INTERFACE listener_base.hpp)

target_include_directories(${DMP_HTTP}.Listeners.Base INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${DMP_HTTP}.Listeners.Base INTERFACE
        ${DMP_HTTP}.Routing.Router
        Boost::asio
)
##############################################################################
