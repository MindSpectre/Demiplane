##############################################################################
# Http Connection — TcpConnection (plain TCP over beast::tcp_stream)
##############################################################################
add_library(${DMP_HTTP}.Connection.Tcp STATIC
        tcp_connection.cpp
)

target_include_directories(${DMP_HTTP}.Connection.Tcp PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${DMP_HTTP}.Connection.Tcp
        PUBLIC
        ${DMP_HTTP}.Connection.RequestArena
        ${DMP_HTTP}.Connection.Concepts
        ${DMP_HTTP}.Types.Enums
        Boost::beast
        Boost::asio
)
##############################################################################
