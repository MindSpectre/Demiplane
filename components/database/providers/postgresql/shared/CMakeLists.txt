##############################################################################
# Small requirable structs
##############################################################################
add_library(${DMP_DATABASE_POSTGRESQL}.Shared INTERFACE
        pg_type_registry.hpp
        pg_format_registry.hpp
)
target_include_directories(${DMP_DATABASE_POSTGRESQL}.Shared INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)
##############################################################################