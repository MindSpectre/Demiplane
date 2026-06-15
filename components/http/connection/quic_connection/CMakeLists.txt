##############################################################################
# Http Connection — QuicConnection (SCAFFOLD; ngtcp2 state lands with h3)
##############################################################################
add_library(${DMP_HTTP}.Connection.Quic INTERFACE
        quic_connection.hpp
)

target_include_directories(${DMP_HTTP}.Connection.Quic INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${DMP_HTTP}.Connection.Quic INTERFACE
        ${DMP_HTTP}.Connection.RequestArena
        ${DMP_HTTP}.Connection.Concepts
        ${DMP_HTTP}.Types.Enums
        Boost::asio
)
##############################################################################
