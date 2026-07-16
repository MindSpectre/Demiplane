##############################################################################
# Http Connection — TlsConnection (ssl::stream<tcp_stream> + arena + ALPN)
##############################################################################
add_library(${DMP_HTTP}.Connection.Tls STATIC tls_connection.cpp)

target_include_directories(${DMP_HTTP}.Connection.Tls PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${DMP_HTTP}.Connection.Tls
        PUBLIC
        ${DMP_HTTP}.Connection.RequestArena
        ${DMP_HTTP}.Connection.Concepts
        ${DMP_HTTP}.Types.Enums
        ${DMP_HTTP}.Types.Executor
        Boost::beast
        Boost::asio
        OpenSSL::SSL
        OpenSSL::Crypto
)
##############################################################################
