##############################################################################
# PostgreSql Dialect
##############################################################################
add_library(${DMP_DATABASE_POSTGRESQL}.Dialect STATIC
        include/postgres_dialect.hpp
        include/pg_sql_type_mapping.hpp
        source/postgres_dialect.cpp
)
target_include_directories(${DMP_DATABASE_POSTGRESQL}.Dialect PUBLIC
        include
)
target_link_libraries(${DMP_DATABASE_POSTGRESQL}.Dialect
        PUBLIC
        ${DMP_DATABASE}.Base.Dialect
        ${DMP_DATABASE}.Base.TypeMapping
        ${DMP_DATABASE_POSTGRESQL}.Shared
        PRIVATE
        Demiplane::Common::Gears
        ${DMP_DATABASE_POSTGRESQL}.Params
)
##############################################################################