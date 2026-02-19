##############################################################################
# Column
##############################################################################
add_library(${DMP_DATABASE}.Primitives.Column STATIC
        db_column.hpp
)

target_include_directories(${DMP_DATABASE}.Primitives.Column PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${DMP_DATABASE}.Primitives.Column PUBLIC
        ${DMP_DATABASE}.Primitives.Field
)
##############################################################################