set(DMP_DATABASE_POSTGRESQL ${DMP_DATABASE_PROVIDERS}.PostgreSQL)

add_subdirectory(shared)

add_subdirectory(dialect)

add_subdirectory(params)

add_subdirectory(result)

add_subdirectory(config)

add_subdirectory(connection)

add_subdirectory(transaction)

add_combined_library(${DMP_DATABASE_POSTGRESQL}
        DIRECTORIES
        export/
        SOURCES
        export/demiplane/postgresql
        LIBRARIES
        ${DMP_DATABASE_POSTGRESQL}.Dialect
        ${DMP_DATABASE_POSTGRESQL}.Params
        ${DMP_DATABASE_POSTGRESQL}.Result
)

add_library(Demiplane::Component::Database::PostgreSQL ALIAS ${DMP_DATABASE_POSTGRESQL})