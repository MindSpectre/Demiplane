##############################################################################
# Core Database Table
##############################################################################
add_library(${DMP_DATABASE}.Primitives.Table STATIC
        include/db_table.hpp
        source/db_table.cpp
        source/db_table.inl
)

target_include_directories(${DMP_DATABASE}.Primitives.Table PUBLIC
        include/
)

target_link_libraries(${DMP_DATABASE}.Primitives.Table
        PUBLIC
        ${DMP_DATABASE}.Primitives.Schema
        PRIVATE
        ${DMP_DATABASE}.Primitives.Field
        ${DMP_DATABASE}.Primitives.Column
)
##############################################################################