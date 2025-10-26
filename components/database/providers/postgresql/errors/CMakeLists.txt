##############################################################################
# PostgreSQL Error Handling Library
#
# Provides comprehensive PostgreSQL error mapping and extraction utilities.
# Changed from INTERFACE to STATIC to compile large SQLSTATE mapping functions.
##############################################################################
add_library(${DMP_DATABASE_POSTGRESQL}.Errors STATIC
        include/postgres_errors.hpp
        source/postgres_errors.cpp
)

target_include_directories(${DMP_DATABASE_POSTGRESQL}.Errors PUBLIC
        include/
)

target_link_libraries(${DMP_DATABASE_POSTGRESQL}.Errors PUBLIC
        ${DMP_DATABASE}.Base.Errors
        PostgreSQL::PostgreSQL
)
##############################################################################