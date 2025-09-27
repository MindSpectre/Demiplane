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
)
##############################################################################



##############################################################################
# Database exceptions base classes
##############################################################################
add_library(${DMP_DATABASE}.Base.Exceptions INTERFACE
        exceptions/db_exceptions.hpp
)
target_include_directories(${DMP_DATABASE}.Base.Exceptions INTERFACE
        exceptions/
)
target_link_libraries(${DMP_DATABASE}.Base.Exceptions INTERFACE
        Boost::exception
        Boost::system
        Boost::stacktrace_backtrace
)
##############################################################################




