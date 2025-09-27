##############################################################################
# Basic SQL expressions for static query building
##############################################################################
add_library(${DMP_DATABASE}.Query.Expressions INTERFACE
        include/query_expressions.hpp
        include/basic.hpp
        include/db_expressions_fwd.hpp
)
target_include_directories(${DMP_DATABASE}.Query.Expressions INTERFACE
        include
)
target_link_libraries(${DMP_DATABASE}.Query.Expressions INTERFACE
        ${DMP_DATABASE}.Primitives
)
##############################################################################