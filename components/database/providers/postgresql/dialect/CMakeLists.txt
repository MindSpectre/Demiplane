##############################################################################
# PostgreSql Dialect
##############################################################################
add_library(${DMP_DATABASE_POSTGRESQL}.Dialect STATIC
        postgres_dialect.hpp
        postgres_dialect.cpp
        detail/postgres_dialect.inl
)
target_include_directories(${DMP_DATABASE_POSTGRESQL}.Dialect PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(${DMP_DATABASE_POSTGRESQL}.Dialect
        PUBLIC
        ${DMP_DATABASE}.Base.Dialect
        ${DMP_DATABASE}.Base.TypeMapping
        ${DMP_DATABASE_POSTGRESQL}.Registries
        PRIVATE
        Demiplane::Common::Gears
        ${DMP_DATABASE_POSTGRESQL}.Params
)
##############################################################################