set(DMP_DB_DIALECT ${DMP_DATABASE}.Dialect)

add_library(${DMP_DB_DIALECT}.Interface INTERFACE)
target_include_directories(${DMP_DB_DIALECT}.Interface INTERFACE
        interface/
)
target_link_libraries(${DMP_DB_DIALECT}.Interface INTERFACE
        ${DMP_DATABASE}.Core
)
##############################################################################
# Sql Dialects
##############################################################################
add_library(${DMP_DB_DIALECT}.PostgreSQL STATIC
        postgresql/include/postgres_dialect.hpp
        postgresql/source/postgres_dialect.cpp
)
target_include_directories(${DMP_DB_DIALECT}.PostgreSQL PUBLIC
        postgresql/include
)
target_link_libraries(${DMP_DB_DIALECT}.PostgreSQL
        PUBLIC
        ${DMP_DB_DIALECT}.Interface
        PRIVATE
        Demiplane::Common::Gears
)
##############################################################################

