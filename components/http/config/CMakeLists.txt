##############################################################################
# Http Config — configuration value types. Per-leaf convention; the dotted
# ${DMP_HTTP}.Config target is an INTERFACE aggregate. PR 4 lands only the
# plain TlsConfig leaf (D1); PR 6 grows this with ServerConfig/ListenerConfig/
# Timeouts/load_server_config and rewrites tls_config into a ConfigInterface.
##############################################################################
add_subdirectory(tls_config)

##############################################################################
# Unified interface aggregate
##############################################################################
add_library(${DMP_HTTP}.Config INTERFACE)

target_link_libraries(${DMP_HTTP}.Config INTERFACE
        ${DMP_HTTP}.Config.TlsConfig
)
##############################################################################
