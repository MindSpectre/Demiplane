##############################################################################
# PostgreSql Transaction
##############################################################################
add_library(${DMP_DATABASE_POSTGRESQL}.Transaction STATIC
        status/transaction_status.hpp
        options/transaction_options.hpp
        options/transaction_options.cpp
        base/transaction.hpp
        base/transaction.cpp
        auto_transaction/auto_transaction.hpp
        auto_transaction/auto_transaction.cpp
        savepoint/savepoint.hpp
        savepoint/savepoint.cpp
        postgres_transaction.hpp
)
target_include_directories(${DMP_DATABASE_POSTGRESQL}.Transaction PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(${DMP_DATABASE_POSTGRESQL}.Transaction
        PUBLIC
        ${DMP_DATABASE_POSTGRESQL}.Capabilities
        ${DMP_DATABASE_POSTGRESQL}.Errors
)
##############################################################################
