##############################################################################
# Http Config — ServerConfig (plain struct; PR 6 promotes to ConfigInterface)
##############################################################################
add_library(${DMP_HTTP}.Config.ServerConfig INTERFACE server_config.hpp)

target_include_directories(${DMP_HTTP}.Config.ServerConfig INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)
##############################################################################
