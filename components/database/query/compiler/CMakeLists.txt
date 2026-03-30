##############################################################################
# Query compiler translates Expression class to std::string + param following chosen SqlDialect
##############################################################################
add_library(${DMP_DATABASE}.Query.Compiler STATIC
        query_compiler.hpp
        param_mode.hpp
        compiled_query/compiled_dynamic_query.hpp
        compiled_query/compiled_static_query.hpp
        query_visitor/query_visitor.hpp
        query_visitor/sql_generator_visitor.hpp
        query_visitor/sql_generator_visitor.cpp
        query_visitor/detail/query_visitor.inl
        query_visitor/detail/sql_generator_visitor.inl
)
target_include_directories(${DMP_DATABASE}.Query.Compiler PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(${DMP_DATABASE}.Query.Compiler
        PUBLIC
        ${DMP_DATABASE}.Query.Expressions
        ${DMP_DATABASE}.Base.Dialect
        ${DMP_DATABASE}.Base.TypeMapping
        Demiplane::Common::Scroll
        PRIVATE
        Demiplane::Common::Nexus
)
##############################################################################