##############################################################################
# Postgres result class + views
##############################################################################
add_library(${DMP_DATABASE_POSTGRESQL}.Result STATIC
        include/pg_result_views.hpp
        include/pg_result.hpp
        source/pg_result.cpp
        source/pg_result_views.cpp

)
target_include_directories(${DMP_DATABASE_POSTGRESQL}.Result PUBLIC
    include/
)

target_link_libraries(${DMP_DATABASE_POSTGRESQL}.Result PUBLIC
        PostgreSQL::PostgreSQL
        Demiplane::Common::Gears
        ${DMP_DATABASE_POSTGRESQL}.Shared
        ${DMP_DATABASE}.Base.Exceptions
        PRIVATE
        ${DMP_DATABASE}.Primitives.Field
)

##############################################################################
