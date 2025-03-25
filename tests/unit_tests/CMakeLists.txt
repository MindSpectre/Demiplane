

##############################################################################
# Test Stopwatch benchmark
##############################################################################
add_unit_test(${UNIT_TESTING_TARGET}.Stopwatch
    stopwatch/test_stopwatch.cpp
)
target_link_libraries(${UNIT_TESTING_TARGET}.Stopwatch
        PRIVATE
        Demiplane::Stopwatch
        ${TEST_NECESSARY_LIBS}
)
##############################################################################

##############################################################################
# Test Transaction Manager
##############################################################################
add_unit_test(${UNIT_TESTING_TARGET}.TransactionManager
        transaction_manager/test_transaction_manager.cpp
)
target_link_libraries(${UNIT_TESTING_TARGET}.TransactionManager
        PRIVATE
        Demiplane::Database
        ${TEST_NECESSARY_LIBS}

)
##############################################################################

##############################################################################
# Test DbInterface Pool
##############################################################################
add_unit_test(${UNIT_TESTING_TARGET}.DbInterfacePool
        db_interface_pool/test_db_interface_pool.cpp
)
target_link_libraries(${UNIT_TESTING_TARGET}.DbInterfacePool
        PRIVATE
        Demiplane::Database
        ${TEST_NECESSARY_LIBS}

)
##############################################################################



##############################################################################
# Test Thread Pool
##############################################################################
add_unit_test(${UNIT_TESTING_TARGET}.ThreadPool
        thread_pool/test_thread_pool.cpp
)
target_link_libraries(${UNIT_TESTING_TARGET}.ThreadPool
        PRIVATE
        Demiplane::Multithread::ThreadPool
        ${TEST_NECESSARY_LIBS}

)
##############################################################################


##############################################################################
# Base utilities objects for database
##############################################################################
add_subdirectory(db_base)
##############################################################################

##############################################################################
# Test Basic DB interface Mock
##############################################################################
add_unit_test(${UNIT_TESTING_TARGET}.DbInterfaceBasicMock
        db_mocks/test_db_basic_mock.cpp
)
target_link_libraries(${UNIT_TESTING_TARGET}.DbInterfaceBasicMock
        PRIVATE
        Demiplane::Database::Mock::Basic
        ${TEST_NECESSARY_LIBS}

)
##############################################################################


##############################################################################
# Test Silent DB interface Mock
##############################################################################
add_unit_test(${UNIT_TESTING_TARGET}.DbInterfaceSilentMock
        db_mocks/test_db_silent_mock.cpp
)
target_link_libraries(${UNIT_TESTING_TARGET}.DbInterfaceSilentMock
        PRIVATE
        Demiplane::Database::Mock::Silent
        ${TEST_NECESSARY_LIBS}

)
##############################################################################

##############################################################################
# Test Silent DB interface Mock
##############################################################################
add_unit_test(${UNIT_TESTING_TARGET}.PqxxQueryEngine
        pqxx_query_engine/pqxx_query_engine.cpp
)
target_link_libraries(${UNIT_TESTING_TARGET}.PqxxQueryEngine
        PRIVATE
        Demiplane::Database::Postgres
        ${TEST_NECESSARY_LIBS}

)
##############################################################################