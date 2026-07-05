##############################################################################
# Http Listeners — QuicListener<Http3Driver> (SCAFFOLD; no ngtcp2/nghttp3, D6)
##############################################################################
add_library(${DMP_HTTP}.Listeners.Quic INTERFACE quic_listener.hpp)

target_include_directories(${DMP_HTTP}.Listeners.Quic INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${DMP_HTTP}.Listeners.Quic INTERFACE
        ${DMP_HTTP}.Listeners.Base
        ${DMP_HTTP}.Drivers.Http3
        ${DMP_HTTP}.Config.TlsConfig
        ${DMP_HTTP}.Routing.Router
        Demiplane::Common::Scroll
        Boost::asio
)
##############################################################################
