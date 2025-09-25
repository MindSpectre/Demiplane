set(DMP_DATABASE_ORM ${DMP_DATABASE}.ORM)

add_subdirectory(compiler)

add_subdirectory(core)

add_subdirectory(expressions)

add_combined_library(${DMP_DATABASE_ORM}
        DIRECTORIES
        export/
        SOURCES
        export/demiplane/orm
        LIBRARIES
        ${DMP_DATABASE_ORM}.Compiler
)

add_library(Demiplane::Component::Database::ORM ALIAS ${DMP_DATABASE_ORM})