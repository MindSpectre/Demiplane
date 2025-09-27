##############################################################################
# Query compiler translates Expression class to std::string + param following chosen SqlDialect
##############################################################################
add_library(${DMP_DATABASE}.Query.Compiler STATIC
        include/query_compiler.hpp
        include/query_visitor.hpp
        include/sql_generator_visitor.hpp
        source/sql_generator_visitor.cpp
        include/compiled_query.hpp
)
target_include_directories(${DMP_DATABASE}.Query.Compiler PUBLIC
        include
)
target_link_libraries(${DMP_DATABASE}.Query.Compiler
        PUBLIC
        ${DMP_DATABASE}.Query.Expressions
        ${DMP_DATABASE}.Base.Dialect
        Demiplane::Common::Scroll
        PRIVATE
        Demiplane::Common::Nexus
)
##############################################################################