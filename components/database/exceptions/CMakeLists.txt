##############################################################################
# Database exceptions class
##############################################################################
add_library(${DMP_DATABASE}.Exceptions STATIC
        source/db_exceptions.cpp
        include/db_exceptions.hpp
)
target_include_directories(${DMP_DATABASE}.Exceptions PUBLIC
        include/
)
target_link_libraries(${DMP_DATABASE}.Exceptions PUBLIC
        Boost::exception
        Boost::system
        Boost::stacktrace_backtrace
)
##############################################################################