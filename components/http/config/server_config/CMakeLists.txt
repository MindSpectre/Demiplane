##############################################################################
# Http Config — ServerConfig (ConfigInterface; JSON-loadable)
##############################################################################
add_library(${DMP_HTTP}.Config.ServerConfig INTERFACE server_config.hpp)

target_include_directories(${DMP_HTTP}.Config.ServerConfig INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${DMP_HTTP}.Config.ServerConfig INTERFACE
        ${DMP_HTTP}.Config.ListenerConfig
        ${DMP_HTTP}.Config.Timeouts
        Demiplane::Common::Serialization
)
##############################################################################
