##############################################################################
# Http Config — configuration value types + JSON loading (spec §10).
# Per-leaf convention; the dotted ${DMP_HTTP}.Config target is an INTERFACE
# aggregate.
##############################################################################
add_subdirectory(tls_config)
add_subdirectory(server_config)
add_subdirectory(timeouts)
add_subdirectory(listener_config)
add_subdirectory(load_server_config)

##############################################################################
# Unified interface aggregate
##############################################################################
add_library(${DMP_HTTP}.Config INTERFACE)

target_link_libraries(${DMP_HTTP}.Config INTERFACE
        ${DMP_HTTP}.Config.TlsConfig
        ${DMP_HTTP}.Config.ServerConfig
        ${DMP_HTTP}.Config.Timeouts
        ${DMP_HTTP}.Config.ListenerConfig
        ${DMP_HTTP}.Config.LoadServerConfig
)
##############################################################################
