##############################################################################
# Http Listeners — transport layer: the ListenerBase polymorphic seam, the
# connection tracker, and concrete listeners (TCP, TLS+ALPN, QUIC scaffold).
# Per-leaf convention; the dotted ${DMP_HTTP}.Listeners target is an INTERFACE
# aggregate. Leaves are added by subsequent tasks.
##############################################################################

add_subdirectory(listener_base)
add_subdirectory(connection_tracker)
add_subdirectory(tcp_listener)
add_subdirectory(tls_listener)
add_subdirectory(quic_listener)

##############################################################################
# Unified interface aggregate
##############################################################################
add_library(${DMP_HTTP}.Listeners INTERFACE)

target_link_libraries(${DMP_HTTP}.Listeners INTERFACE
        ${DMP_HTTP}.Listeners.Base
        ${DMP_HTTP}.Listeners.ConnectionTracker
        ${DMP_HTTP}.Listeners.Tcp
        ${DMP_HTTP}.Listeners.Tls
        ${DMP_HTTP}.Listeners.Quic
)
##############################################################################
