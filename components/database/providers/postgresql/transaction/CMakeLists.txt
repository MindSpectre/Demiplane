##############################################################################
# PostgreSql Transaction
##############################################################################
add_library(${DMP_DATABASE_POSTGRESQL}.Transaction STATIC
        include/postgres_transaction.hpp
        source/postgres_transaction.cpp
)
target_include_directories(${DMP_DATABASE_POSTGRESQL}.Transaction PUBLIC
        include
)
target_link_libraries(${DMP_DATABASE_POSTGRESQL}.Transaction
        PUBLIC
        ${DMP_DATABASE_POSTGRESQL}.Connection
)
##############################################################################