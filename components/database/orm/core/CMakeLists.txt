set(DMP_DATABASE_ORM_CORE ${DMP_DATABASE_ORM}.Core)

add_subdirectory(column)
add_subdirectory(field)
add_subdirectory(record)
add_subdirectory(table)

##############################################################################
# Fundamental structures for orm
##############################################################################
add_combined_library(${DMP_DATABASE_ORM_CORE}
        DIRECTORIES
        export/
        SOURCES
        export/db_core_objects.hpp
        LIBRARIES
        ${DMP_DATABASE_CORE}.Field
        ${DMP_DATABASE_CORE}.Column
        ${DMP_DATABASE_CORE}.Record
        ${DMP_DATABASE_CORE}.Table
)
##############################################################################