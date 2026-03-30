##############################################################################
# DB Row Record (hard linked to table)
##############################################################################
add_library(${DMP_DATABASE}.Primitives.Record STATIC
        db_record.hpp
        db_record_factory.hpp
        db_record.cpp
)
target_include_directories(${DMP_DATABASE}.Primitives.Record PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(${DMP_DATABASE}.Primitives.Record
        PRIVATE
        ${DMP_DATABASE}.Primitives.Field
        ${DMP_DATABASE}.Primitives.Table
)
##############################################################################