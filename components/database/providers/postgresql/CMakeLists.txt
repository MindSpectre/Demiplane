set(DMP_DATABASE_POSTGRESQL ${DMP_DATABASE_PROVIDERS}.PostgreSQL)

add_subdirectory(shared)

add_subdirectory(dialect)

add_subdirectory(params)

add_subdirectory(result)

add_combined_library(${DMP_DATABASE_POSTGRESQL}
        DIRECTORIES
        export/
        SOURCES
        export/demiplane/postgresql
        LIBRARIES
        ${DMP_DATABASE_POSTGRESQL}.Dialect
)

add_library(Demiplane::Component::Database::PostgreSQL ALIAS ${DMP_DATABASE_POSTGRESQL})