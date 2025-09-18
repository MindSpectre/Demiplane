##############################################################################
# Basic SQL expressions for static query building
##############################################################################
add_library(${DMP_DATABASE_ORM}.Expressions INTERFACE
        include/query_expressions.hpp
        include/basic.hpp
        include/db_expressions_fwd.hpp
)
target_include_directories(${DMP_DATABASE_ORM}.Expressions INTERFACE
        include
)
target_link_libraries(${DMP_DATABASE_ORM}.Expressions INTERFACE
        ${DMP_DATABASE_ORM}.Core
)
##############################################################################