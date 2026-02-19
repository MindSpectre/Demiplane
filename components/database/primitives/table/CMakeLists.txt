##############################################################################
# Core Database Table
##############################################################################
add_library(${DMP_DATABASE}.Primitives.Table STATIC
        db_table.hpp
        db_table.cpp
        detail/db_table.inl
)

target_include_directories(${DMP_DATABASE}.Primitives.Table PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${DMP_DATABASE}.Primitives.Table
        PUBLIC
        ${DMP_DATABASE}.Primitives.Schema
        ${DMP_DATABASE}.Features
        PRIVATE
        ${DMP_DATABASE}.Primitives.Field
        ${DMP_DATABASE}.Primitives.Column
)
##############################################################################