##############################################################################
# Http Config — TlsConfig (plain struct; PR 6 promotes to ConfigInterface, D1)
##############################################################################
add_library(${DMP_HTTP}.Config.TlsConfig INTERFACE tls_config.hpp)

target_include_directories(${DMP_HTTP}.Config.TlsConfig INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)
##############################################################################
