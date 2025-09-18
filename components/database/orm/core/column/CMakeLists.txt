##############################################################################
# Column
##############################################################################
add_library(${DMP_DATABASE_CORE}.Column STATIC
        include/db_column.hpp
        source/db_column.cpp
)

target_include_directories(${DMP_DATABASE_CORE}.Column PUBLIC
        include/
)

target_link_libraries(${DMP_DATABASE_CORE}.Column PUBLIC
        ${DMP_DATABASE_CORE}.Field
)
##############################################################################