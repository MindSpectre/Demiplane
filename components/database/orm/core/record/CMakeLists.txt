##############################################################################
# DB Row Record (hard linked to table)
##############################################################################
add_library(${DMP_DATABASE_CORE}.Record STATIC
        include/db_record.hpp
        include/db_record_factory.hpp
        source/db_record.cpp
)
target_include_directories(${DMP_DATABASE_CORE}.Record PUBLIC
        include
)
target_link_libraries(${DMP_DATABASE_CORE}.Record
        PRIVATE
        ${DMP_DATABASE_CORE}.Field
        ${DMP_DATABASE_CORE}.Table
)
##############################################################################