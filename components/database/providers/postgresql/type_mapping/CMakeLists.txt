##############################################################################
# PostgreSql Type Mapping
##############################################################################
add_library(${DMP_DATABASE_POSTGRESQL}.TypeMapping STATIC
        include/postgres_type_mapping.hpp

)
target_include_directories(${DMP_DATABASE_POSTGRESQL}.TypeMapping PUBLIC
        include
)
target_link_libraries(${DMP_DATABASE_POSTGRESQL}.TypeMapping
        PUBLIC
        ${DMP_DATABASE}.Base.TypeMapping
        ${DMP_DATABASE_POSTGRESQL}.Shared
)
##############################################i################################