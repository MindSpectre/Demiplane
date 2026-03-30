##############################################################################
# Small requirable structs
##############################################################################
add_library(${DMP_DATABASE_POSTGRESQL}.Registries INTERFACE
        postgres_oid_type_registry.hpp
        postgres_format_registry.hpp
        postgres_sql_type_registry.hpp
)
target_include_directories(${DMP_DATABASE_POSTGRESQL}.Registries INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)
##############################################################################
