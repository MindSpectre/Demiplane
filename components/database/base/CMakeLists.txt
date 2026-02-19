##############################################################################
# SQL Type Mapping (C++ type -> SQL type string)
# Note: Depends on Dialect because .inl needs sql_dialect.hpp for type()
##############################################################################
add_library(${DMP_DATABASE}.Base.TypeMapping INTERFACE
        type_mapping/sql_type_mapping.hpp
        type_mapping/detail/sql_type_mapping.inl
        type_mapping/detail/db_table_mapping.inl
)
target_include_directories(${DMP_DATABASE}.Base.TypeMapping INTERFACE
        type_mapping/
)
target_link_libraries(${DMP_DATABASE}.Base.TypeMapping INTERFACE
        ${DMP_DATABASE}.Features
        ${DMP_DATABASE}.Base.Dialect
)
##############################################################################


##############################################################################
# Sql Params base class
##############################################################################
add_library(${DMP_DATABASE}.Base.Params INTERFACE
        params/sql_params.hpp
)
target_include_directories(${DMP_DATABASE}.Base.Params INTERFACE
        params/
)
target_link_libraries(${DMP_DATABASE}.Base.Params INTERFACE
        ${DMP_DATABASE}.Primitives
)

##############################################################################


##############################################################################
# Dialect base class
##############################################################################
add_library(${DMP_DATABASE}.Base.Dialect INTERFACE
        dialect/sql_dialect.hpp
)
target_include_directories(${DMP_DATABASE}.Base.Dialect INTERFACE
        dialect/
)
target_link_libraries(${DMP_DATABASE}.Base.Dialect INTERFACE
        ${DMP_DATABASE}.Base.Params
        ${DMP_DATABASE}.Features
)
##############################################################################

##############################################################################
# Database exceptions base classes
##############################################################################
add_library(${DMP_DATABASE}.Base.Errors STATIC
        errors/db_exceptions.hpp
        errors/db_error_codes.hpp
)
target_include_directories(${DMP_DATABASE}.Base.Errors PUBLIC
        errors/
)
target_link_libraries(${DMP_DATABASE}.Base.Errors PUBLIC
        Boost::exception
        Boost::system
        Boost::stacktrace_backtrace
)
##############################################################################




