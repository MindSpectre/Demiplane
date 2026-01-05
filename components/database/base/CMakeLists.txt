##############################################################################
# Database support info
##############################################################################
add_library(${DMP_DATABASE}.Base.SupportInfo INTERFACE
        support/supported_providers.hpp
)
target_include_directories(${DMP_DATABASE}.Base.SupportInfo INTERFACE
        support/
)
target_link_libraries(${DMP_DATABASE}.Base.SupportInfo INTERFACE
)
##############################################################################


##############################################################################
# SQL Type Mapping (C++ type -> SQL type string)
# Note: Depends on Dialect because .inl needs sql_dialect.hpp for type()
##############################################################################
add_library(${DMP_DATABASE}.Base.TypeMapping INTERFACE
        type_mapping/include/sql_type_mapping.hpp
        type_mapping/source/sql_type_mapping.inl
)
target_include_directories(${DMP_DATABASE}.Base.TypeMapping INTERFACE
        type_mapping/include
)
target_link_libraries(${DMP_DATABASE}.Base.TypeMapping INTERFACE
        ${DMP_DATABASE}.Base.SupportInfo
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
        ${DMP_DATABASE}.Base.SupportInfo
)
##############################################################################


##############################################################################
# Database exceptions base classes
##############################################################################
add_library(${DMP_DATABASE}.Base.Errors STATIC
        errors/include/db_exceptions.hpp
        errors/include/db_error_codes.hpp
)
target_include_directories(${DMP_DATABASE}.Base.Errors PUBLIC
        errors/include
)
target_link_libraries(${DMP_DATABASE}.Base.Errors PUBLIC
        Boost::exception
        Boost::system
        Boost::stacktrace_backtrace
)
##############################################################################




