set(DMP_DATABASE_QUERY ${DMP_DATABASE}.Query)
##############################################################################
# Basic SQL expressions for static query building
##############################################################################
add_library(${DMP_DATABASE_QUERY}.Expressions INTERFACE
        expressions/query_expressions.hpp
        expressions/basic.hpp
        expressions/db_expressions_fwd.hpp
)
target_include_directories(${DMP_DATABASE_QUERY}.Expressions INTERFACE
        expressions
)
target_link_libraries(${DMP_DATABASE_QUERY}.Expressions INTERFACE
        ${DMP_DATABASE}.Core
)
##############################################################################

##############################################################################
# Query compiler translates Expression class to std::string + param following chosen SqlDialect
##############################################################################
add_library(${DMP_DATABASE_QUERY}.Compiler STATIC
        compiler/include/query_compiler.hpp
        compiler/include/query_visitor.hpp
        compiler/include/sql_generator_visitor.hpp
        compiler/source/sql_generator_visitor.cpp
)
target_include_directories(${DMP_DATABASE_QUERY}.Compiler PUBLIC
        compiler/include
)
target_link_libraries(${DMP_DATABASE_QUERY}.Compiler PUBLIC
        ${DMP_DATABASE_QUERY}.Expressions
        ${DMP_DATABASE}.Dialect.Interface
        Demiplane::Common::Scroll
        PRIVATE
        Demiplane::Common::Nexus
)
##############################################################################