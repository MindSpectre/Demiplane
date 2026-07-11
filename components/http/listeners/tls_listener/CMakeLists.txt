##############################################################################
# Http Listeners — TlsListener<Drivers...> + build_ssl_context (TCP+TLS+ALPN)
##############################################################################
add_library(${DMP_HTTP}.Listeners.Tls STATIC
        build_ssl_context.cpp
        tls_listener.hpp
)

target_include_directories(${DMP_HTTP}.Listeners.Tls PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${DMP_HTTP}.Listeners.Tls
        PUBLIC
        ${DMP_HTTP}.Listeners.Base
        ${DMP_HTTP}.Listeners.ConnectionTracker
        ${DMP_HTTP}.Connection.Tls
        ${DMP_HTTP}.Types.Executor
        ${DMP_HTTP}.Config.TlsConfig
        ${DMP_HTTP}.Drivers.Concept
        ${DMP_HTTP}.Routing.Router
        Demiplane::Common::Chrono
        Boost::beast
        Boost::asio
        OpenSSL::SSL
        OpenSSL::Crypto
)
##############################################################################
