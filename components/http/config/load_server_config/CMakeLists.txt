##############################################################################
# Http Config — load_server_config / dump_server_config + config error types
##############################################################################
add_library(${DMP_HTTP}.Config.LoadServerConfig STATIC
        load_server_config.cpp
        load_server_config.hpp
)

target_include_directories(${DMP_HTTP}.Config.LoadServerConfig PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${DMP_HTTP}.Config.LoadServerConfig
        PUBLIC
        ${DMP_HTTP}.Config.ServerConfig
        ${DMP_HTTP}.Types
        Demiplane::Common::Gears
        Demiplane::Common::Serialization
        JsonCpp::JsonCpp
)
##############################################################################
