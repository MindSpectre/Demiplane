add_subdirectory(compiler)

add_subdirectory(expressions)

add_combined_library(${DMP_DATABASE}.Query
        DIRECTORIES
        export/
        SOURCES
        export/demiplane/orm
        LIBRARIES
        ${DMP_DATABASE}.Query.Compiler
)