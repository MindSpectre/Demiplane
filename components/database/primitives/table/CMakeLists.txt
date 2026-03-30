##############################################################################
# Core Database Table
##############################################################################
add_library(${DMP_DATABASE}.Primitives.Table STATIC
        db_table.hpp
        db_dynamic_table.cpp
        db_static_table.hpp
        db_dynamic_table.hpp
        detail/db_dynamic_table.inl
)

target_include_directories(${DMP_DATABASE}.Primitives.Table PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${DMP_DATABASE}.Primitives.Table
        PUBLIC
        ${DMP_DATABASE}.Features
        ${DMP_DATABASE}.Primitives.Column
        ${DMP_DATABASE}.Primitives.Field
)
##############################################################################