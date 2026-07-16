##############################################################################
# Http Listeners — TcpListener<Driver> (plain TCP; header-only template)
##############################################################################
add_library(${DMP_HTTP}.Listeners.Tcp INTERFACE tcp_listener.hpp)

target_include_directories(${DMP_HTTP}.Listeners.Tcp INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${DMP_HTTP}.Listeners.Tcp INTERFACE
        ${DMP_HTTP}.Listeners.Base
        ${DMP_HTTP}.Listeners.ConnectionTracker
        ${DMP_HTTP}.Connection.Tcp
        ${DMP_HTTP}.Types.Executor
        ${DMP_HTTP}.Drivers.Concept
        ${DMP_HTTP}.Routing.Router
        Demiplane::Common::Chrono
        Boost::beast
        Boost::asio
)
##############################################################################
