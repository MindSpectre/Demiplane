set(DMP_DATABASE_DIALECT ${DMP_DATABASE}.Dialect)

add_library(${DMP_DATABASE_DIALECT}.Interface INTERFACE
        interface/sql_dialect.hpp
)
target_include_directories(${DMP_DATABASE_DIALECT}.Interface INTERFACE
        interface/
)
target_link_libraries(${DMP_DATABASE_DIALECT}.Interface INTERFACE
        ${DMP_DATABASE}.Core
)
##############################################################################
# Sql Dialects
##############################################################################
add_library(${DMP_DATABASE_DIALECT}.PostgreSQL STATIC
        postgresql/include/postgres_dialect.hpp
        postgresql/source/postgres_dialect.cpp
)
target_include_directories(${DMP_DATABASE_DIALECT}.PostgreSQL PUBLIC
        postgresql/include
)
target_link_libraries(${DMP_DATABASE_DIALECT}.PostgreSQL
        PUBLIC
        ${DMP_DATABASE_DIALECT}.Interface
        PRIVATE
        Demiplane::Common::Gears
)
##############################################################################

