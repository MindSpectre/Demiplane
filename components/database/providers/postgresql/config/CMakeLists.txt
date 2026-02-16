##############################################################################
# PostgreSql Config
##############################################################################
add_library(${DMP_DATABASE_POSTGRESQL}.Config STATIC
        include/postgres_connection_credentials.hpp
        include/postgres_connection_config.hpp
        include/postgres_ssl_mode.hpp
        include/postgres_nodes_role.hpp
        include/postgres_connection_tools.hpp
        source/postgres_connection_config.cpp
        source/postgres_connection_credentials.cpp
)
target_include_directories(${DMP_DATABASE_POSTGRESQL}.Config PUBLIC
        include
)
target_link_libraries(${DMP_DATABASE_POSTGRESQL}.Config
        PUBLIC
        ${DMP_DATABASE_POSTGRESQL}.Shared
        Demiplane::Common::Gears
        JsonCpp::JsonCpp
)
##############################################################################