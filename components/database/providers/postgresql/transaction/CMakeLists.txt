##############################################################################
# PostgreSql Transaction
##############################################################################
add_library(${DMP_DATABASE_POSTGRESQL}.Transaction STATIC
        status/transaction_status.hpp
        options/transaction_options.hpp
        options/transaction_options.cpp
        manual_transaction/manual_transaction.hpp
        manual_transaction/manual_transaction.cpp
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
        ${DMP_DATABASE_POSTGRESQL}.ConnectionSlot
        ${DMP_DATABASE_POSTGRESQL}.Capabilities.Executor.Sync
        ${DMP_DATABASE_POSTGRESQL}.Capabilities.Executor.Async
        ${DMP_DATABASE_POSTGRESQL}.Errors
        Boost::asio
)
##############################################################################
