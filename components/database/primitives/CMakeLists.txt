add_subdirectory(column)
add_subdirectory(field)
add_subdirectory(record)
add_subdirectory(schema)
add_subdirectory(table)

##############################################################################
# Fundamental structures for orm
##############################################################################
add_combined_library(${DMP_DATABASE}.Primitives
        DIRECTORIES
        export/
        SOURCES
        export/db_core_objects.hpp
        LIBRARIES
        ${DMP_DATABASE}.Primitives.Field
        ${DMP_DATABASE}.Primitives.Column
        ${DMP_DATABASE}.Primitives.Record
        ${DMP_DATABASE}.Primitives.Schema
        ${DMP_DATABASE}.Primitives.Table
)
##############################################################################