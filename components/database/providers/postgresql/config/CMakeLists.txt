##############################################################################
# PostgreSql Config
##############################################################################
add_library(${DMP_DATABASE_POSTGRESQL}.Config STATIC
        include/postgres_connection_config.hpp
        source/postgres_connection_config.cpp
        include/postgres_database_config.hpp
)
target_include_directories(${DMP_DATABASE_POSTGRESQL}.Config PUBLIC
        include
)
target_link_libraries(${DMP_DATABASE_POSTGRESQL}.Config
        PUBLIC
        ${DMP_DATABASE_POSTGRESQL}.Shared
)
##############################################################################