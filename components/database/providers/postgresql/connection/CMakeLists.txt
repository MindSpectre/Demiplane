##############################################################################
# PostgreSql Connection
##############################################################################
add_library(${DMP_DATABASE_POSTGRESQL}.Connection STATIC
        include/postgres_connection.hpp
        source/postgres_connection.cpp
)
target_include_directories(${DMP_DATABASE_POSTGRESQL}.Connection PUBLIC
        include
)
target_link_libraries(${DMP_DATABASE_POSTGRESQL}.Connection
        PUBLIC
        ${DMP_DATABASE_POSTGRESQL}.Config
        ${DMP_DATABASE_POSTGRESQL}.Result
        ${DMP_DATABASE_POSTGRESQL}.Params
        ${DMP_DATABASE_POSTGRESQL}.Shared
        ${DMP_DATABASE}.Query.Compiler
        Boost::asio
)
##############################################################################