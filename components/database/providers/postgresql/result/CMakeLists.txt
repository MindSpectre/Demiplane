##############################################################################
# Postgres result class + views
##############################################################################
add_library(${DMP_DATABASE_POSTGRESQL}.Result STATIC
        postgres_result.hpp
        views/postgres_result_views.hpp
        views/postgres_result_views.cpp
)
target_include_directories(${DMP_DATABASE_POSTGRESQL}.Result PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${DMP_DATABASE_POSTGRESQL}.Result PUBLIC
        ${DMP_DATABASE_POSTGRESQL}.Registries
        ${DMP_DATABASE}.Base.Errors
        Demiplane::Common::Gears
        PostgreSQL::PostgreSQL
        PRIVATE
        ${DMP_DATABASE}.Primitives.Field
)

##############################################################################
