##############################################################################
# DB Row Record (hard linked to table)
##############################################################################
add_library(${DMP_DATABASE}.Primitives.Record STATIC
        include/db_record.hpp
        include/db_record_factory.hpp
        source/db_record.cpp
)
target_include_directories(${DMP_DATABASE}.Primitives.Record PUBLIC
        include
)
target_link_libraries(${DMP_DATABASE}.Primitives.Record
        PRIVATE
        ${DMP_DATABASE}.Primitives.Field
        ${DMP_DATABASE}.Primitives.Table
)
##############################################################################