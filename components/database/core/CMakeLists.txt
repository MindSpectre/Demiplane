set(DMP_DATABASE_CORE ${DMP_DATABASE}.Core)

add_subdirectory(column)
add_subdirectory(field)
add_subdirectory(row)
add_subdirectory(table)
add_subdirectory(block)

##############################################################################
# Fundamental structures for orm
##############################################################################
add_combined_library(${DMP_DATABASE_CORE}
        DIRECTORIES
        export/
        SOURCES
        export/db_core_objects.hpp
        LIBRARIES
        ${DMP_DATABASE_CORE}.Field
        ${DMP_DATABASE_CORE}.Column
        ${DMP_DATABASE_CORE}.Row.Interface
        ${DMP_DATABASE_CORE}.Row.Record
        ${DMP_DATABASE_CORE}.Table
        ${DMP_DATABASE_CORE}.Block.Interface
)
##############################################################################