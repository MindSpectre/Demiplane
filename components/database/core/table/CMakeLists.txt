##############################################################################
# Core Database Table
##############################################################################
add_library(${DMP_DATABASE_CORE}.Table STATIC
        include/db_table.hpp
        source/db_table.cpp
        source/db_table.inl
)

target_include_directories(${DMP_DATABASE_CORE}.Table PUBLIC
        include/
)

target_link_libraries(${DMP_DATABASE_CORE}.Table PRIVATE
        ${DMP_DATABASE_CORE}.Field
        ${DMP_DATABASE_CORE}.Column
)
##############################################################################