##############################################################################
# Postgres result class + views
##############################################################################
add_library(${DMP_DATABASE_POSTGRESQL}.Result STATIC
        include/postgres_result_views.hpp
        include/postgres_result.hpp
        source/postgres_result.cpp
        source/postgres_result_views.cpp

)
target_include_directories(${DMP_DATABASE_POSTGRESQL}.Result PUBLIC
    include/
)

target_link_libraries(${DMP_DATABASE_POSTGRESQL}.Result PUBLIC
        PostgreSQL::PostgreSQL
        Demiplane::Common::Gears
        ${DMP_DATABASE_POSTGRESQL}.Shared
        ${DMP_DATABASE}.Base.Errors
        PRIVATE
        ${DMP_DATABASE}.Primitives.Field
)

##############################################################################
