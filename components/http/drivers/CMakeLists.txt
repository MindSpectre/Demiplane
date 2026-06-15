##############################################################################
# Http Drivers — the HttpDriver concept + protocol drivers (h1 real, h2/h3
# scaffolds). Per-leaf convention; the dotted ${DMP_HTTP}.Drivers target is an
# INTERFACE aggregate. Leaves are added by subsequent tasks.
##############################################################################
add_subdirectory(http_driver_concept)
add_subdirectory(http11)
add_subdirectory(http2)
add_subdirectory(http3)

##############################################################################
# Unified interface aggregate
##############################################################################
add_library(${DMP_HTTP}.Drivers INTERFACE)

target_link_libraries(${DMP_HTTP}.Drivers INTERFACE
        ${DMP_HTTP}.Drivers.Concept
        ${DMP_HTTP}.Drivers.Http11
        ${DMP_HTTP}.Drivers.Http2
        ${DMP_HTTP}.Drivers.Http3
)
##############################################################################
