set(DMP_DATABASE_POSTGRESQL ${DMP_DATABASE_PROVIDERS}.PostgreSQL)

add_subdirectory(registries)

add_subdirectory(dialect)

add_subdirectory(type_mapping)

add_subdirectory(params)

add_subdirectory(errors)

add_subdirectory(result)

add_subdirectory(config)

add_subdirectory(connection_holder)

add_subdirectory(capabilities)

add_subdirectory(session)

add_subdirectory(transaction)

add_combined_library(${DMP_DATABASE_POSTGRESQL}
        DIRECTORIES
        export/
        SOURCES
        export/demiplane/postgresql
        LIBRARIES
        ${DMP_DATABASE_POSTGRESQL}.Config
        ${DMP_DATABASE_POSTGRESQL}.Dialect
        ${DMP_DATABASE_POSTGRESQL}.Params
        ${DMP_DATABASE_POSTGRESQL}.Result
        ${DMP_DATABASE_POSTGRESQL}.Capabilities
        ${DMP_DATABASE_POSTGRESQL}.TypeMapping
        ${DMP_DATABASE_POSTGRESQL}.Session
        ${DMP_DATABASE_POSTGRESQL}.Transaction
)

add_library(Demiplane::Component::Database::PostgreSQL ALIAS ${DMP_DATABASE_POSTGRESQL})