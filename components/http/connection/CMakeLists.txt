##############################################################################
# Http Connection — per-connection arena, the Connection concept, concrete
# connections. Per-leaf convention (each thing owns its include dir); the
# dotted ${DMP_HTTP}.Connection target is an INTERFACE aggregate. Leaves are
# added by subsequent tasks.
##############################################################################
add_subdirectory(request_arena)
add_subdirectory(concepts)
add_subdirectory(tcp_connection)
add_subdirectory(quic_connection)

##############################################################################
# Unified interface aggregate
##############################################################################
add_library(${DMP_HTTP}.Connection INTERFACE)

target_link_libraries(${DMP_HTTP}.Connection INTERFACE
        ${DMP_HTTP}.Connection.RequestArena
        ${DMP_HTTP}.Connection.Concepts
        ${DMP_HTTP}.Connection.Tcp
        ${DMP_HTTP}.Connection.Quic
)
##############################################################################
