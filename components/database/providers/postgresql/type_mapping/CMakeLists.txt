##############################################################################
# PostgreSql Type Mapping
##############################################################################
add_library(${DMP_DATABASE_POSTGRESQL}.TypeMapping STATIC
        postgres_type_mapping.hpp
)
target_include_directories(${DMP_DATABASE_POSTGRESQL}.TypeMapping PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(${DMP_DATABASE_POSTGRESQL}.TypeMapping
        PUBLIC
        ${DMP_DATABASE}.Base.TypeMapping
        ${DMP_DATABASE_POSTGRESQL}.Registries
)
##############################################i################################