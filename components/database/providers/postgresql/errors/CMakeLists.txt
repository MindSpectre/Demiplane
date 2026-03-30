##############################################################################
# PostgreSQL Error Handling Library
#
# Provides comprehensive PostgreSQL error mapping and extraction utilities.
# Changed from INTERFACE to STATIC to compile large SQLSTATE mapping functions.
##############################################################################
add_library(${DMP_DATABASE_POSTGRESQL}.Errors STATIC
        postgres_errors.hpp
        postgres_errors.cpp
)

target_include_directories(${DMP_DATABASE_POSTGRESQL}.Errors PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${DMP_DATABASE_POSTGRESQL}.Errors PUBLIC
        ${DMP_DATABASE}.Base.Errors
        PostgreSQL::PostgreSQL
)
##############################################################################