##############################################################################
# PostgreSql Config
##############################################################################
add_library(${DMP_DATABASE_POSTGRESQL}.Config STATIC
        include/postgres_config.hpp
        source/postgres_config.cpp
)
target_include_directories(${DMP_DATABASE_POSTGRESQL}.Config PUBLIC
        include
)
target_link_libraries(${DMP_DATABASE_POSTGRESQL}.Config
        PUBLIC
        ${DMP_DATABASE_POSTGRESQL}.Shared
)
##############################################################################