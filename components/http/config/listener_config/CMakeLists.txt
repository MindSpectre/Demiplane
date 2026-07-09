##############################################################################
# Http Config — ListenerConfig (+ Protocol/Transport string codecs)
##############################################################################
add_library(${DMP_HTTP}.Config.ListenerConfig INTERFACE listener_config.hpp)

target_include_directories(${DMP_HTTP}.Config.ListenerConfig INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${DMP_HTTP}.Config.ListenerConfig INTERFACE
        ${DMP_HTTP}.Config.TlsConfig
        ${DMP_HTTP}.Types.Enums
        Demiplane::Common::Serialization
)
##############################################################################
