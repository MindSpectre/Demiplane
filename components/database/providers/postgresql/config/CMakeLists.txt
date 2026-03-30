##############################################################################
# PostgreSql Config
##############################################################################
add_library(${DMP_DATABASE_POSTGRESQL}.Config STATIC
        credentials/postgres_connection_credentials.hpp
        credentials/postgres_connection_credentials.cpp
        postgres_connection_config.hpp
        postgres_connection_config.cpp
        tools/postgres_ssl_mode.hpp
        tools/postgres_nodes_role.hpp
        tools/postgres_connection_tools.hpp
)
target_include_directories(${DMP_DATABASE_POSTGRESQL}.Config PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(${DMP_DATABASE_POSTGRESQL}.Config
        PUBLIC
        ${DMP_DATABASE_POSTGRESQL}.Registries
        Demiplane::Common::Gears
        JsonCpp::JsonCpp
)
##############################################################################