set(DMP_DATABASE_POSTGRESQL ${DMP_DATABASE_CLIENTS}.PostgreSQL)


add_subdirectory(dialect)


add_combined_library(${DMP_DATABASE_POSTGRESQL}
        DIRECTORIES
        export/
        SOURCES
        export/demiplane/postgresql
        LIBRARIES
        ${DMP_DATABASE_POSTGRESQL}.Dialect
)

add_library(Demiplane::Component::Database::PostgreSQL ALIAS ${DMP_DATABASE_POSTGRESQL})