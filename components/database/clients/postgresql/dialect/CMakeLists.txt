##############################################################################
# PostgreSql Dialect
##############################################################################
add_library(${DMP_DATABASE_POSTGRESQL}.Dialect STATIC
        include/postgres_dialect.hpp
        source/postgres_dialect.cpp
)
target_include_directories(${DMP_DATABASE_POSTGRESQL}.Dialect PUBLIC
        include
)
target_link_libraries(${DMP_DATABASE_POSTGRESQL}.Dialect
        PUBLIC
        ${DMP_DATABASE}.Dialect
        PRIVATE
        Demiplane::Common::Gears
)
##############################################################################