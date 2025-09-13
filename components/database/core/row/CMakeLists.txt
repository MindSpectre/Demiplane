##############################################################################
# DB Row Interface
##############################################################################
add_library(${DMP_DATABASE_CORE}.Row.Interface INTERFACE
        interface/
)
target_include_directories(${DMP_DATABASE_CORE}.Row.Interface INTERFACE
        interface/
)
##############################################################################

##############################################################################
# DB Row Record (hard linked to table)
##############################################################################
add_library(${DMP_DATABASE_CORE}.Row.Record STATIC
        record/include/db_record.hpp
        record/include/db_record_factory.hpp
        record/source/db_record.cpp
)
target_include_directories(${DMP_DATABASE_CORE}.Row.Record PUBLIC
        record/include
)
target_link_libraries(${DMP_DATABASE_CORE}.Row.Record
        PUBLIC
        ${DMP_DATABASE_CORE}.Row.Interface
        PRIVATE
        ${DMP_DATABASE_CORE}.Field
        ${DMP_DATABASE_CORE}.Table
)
##############################################################################