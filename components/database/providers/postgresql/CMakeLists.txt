set(DMP_DATABASE_POSTGRESQL ${DMP_DATABASE_PROVIDERS}.PostgreSQL)

add_subdirectory(shared)

add_subdirectory(dialect)

add_subdirectory(params)

add_subdirectory(errors)

add_subdirectory(result)

add_subdirectory(config)

add_subdirectory(capabilities)

add_subdirectory(session)

#add_subdirectory(transaction)

add_combined_library(${DMP_DATABASE_POSTGRESQL}
        DIRECTORIES
        export/
        SOURCES
        export/demiplane/postgresql
        LIBRARIES
        ${DMP_DATABASE_POSTGRESQL}.Dialect
        ${DMP_DATABASE_POSTGRESQL}.Params
        ${DMP_DATABASE_POSTGRESQL}.Result
        ${DMP_DATABASE_POSTGRESQL}.Capabilities
)

add_library(Demiplane::Component::Database::PostgreSQL ALIAS ${DMP_DATABASE_POSTGRESQL})