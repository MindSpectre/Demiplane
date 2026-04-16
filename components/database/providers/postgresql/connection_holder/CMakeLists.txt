##############################################################################
# PostgreSql ConnectionHolder (INTERFACE — shared by executors and pools)
##############################################################################
add_library(${DMP_DATABASE_POSTGRESQL}.ConnectionHolder INTERFACE
        connection_holder.hpp
)
target_include_directories(${DMP_DATABASE_POSTGRESQL}.ConnectionHolder INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(${DMP_DATABASE_POSTGRESQL}.ConnectionHolder INTERFACE
        PostgreSQL::PostgreSQL
)
##############################################################################
