##############################################################################
# Test Stopwatch benchmark
##############################################################################
add_executable(${UNIT_TESTING_TARGET}_StopWatch
        stopwatch/test_stopwatch.cpp
)
target_link_libraries(${UNIT_TESTING_TARGET}_StopWatch
        PRIVATE
        Demiplane::Stopwatch
        ${TEST_NECESSARY_LIBS}

)
add_test(UnitTest_StopWatch ${UNIT_TESTING_TARGET}_StopWatch)
##############################################################################

##############################################################################
# Test Transaction Manager
##############################################################################
add_executable(${UNIT_TESTING_TARGET}_TransactionManager
        transaction_manager/test_transaction_manager.cpp
)
target_link_libraries(${UNIT_TESTING_TARGET}_TransactionManager
        PRIVATE
        Demiplane::Database
        ${TEST_NECESSARY_LIBS}

)
add_test(UnitTest_TransactionManager ${UNIT_TESTING_TARGET}_TransactionManager)
##############################################################################

##############################################################################
# Test DbInterface Pool
##############################################################################
add_executable(${UNIT_TESTING_TARGET}_DbInterfacePool
        db_interface_pool/test_db_interface_pool.cpp
)
target_link_libraries(${UNIT_TESTING_TARGET}_DbInterfacePool
        PRIVATE
        Demiplane::Database
        ${TEST_NECESSARY_LIBS}

)
add_test(UnitTest_DbInterfacePool ${UNIT_TESTING_TARGET}_DbInterfacePool)
##############################################################################



##############################################################################
# Test Thread Pool
##############################################################################
add_executable(${UNIT_TESTING_TARGET}_ThreadPool
        thread_pool/test_thread_pool.cpp
)
target_link_libraries(${UNIT_TESTING_TARGET}_ThreadPool
        PRIVATE
        Demiplane::Multithread::ThreadPool
        ${TEST_NECESSARY_LIBS}

)
add_test(UnitTest_ThreadPool ${UNIT_TESTING_TARGET}_ThreadPool)
##############################################################################


##############################################################################
# Base utilities objects for database
##############################################################################
add_subdirectory(db_base)
##############################################################################

set_tests_properties(
        UnitTest_StopWatch
        UnitTest_TransactionManager
        UnitTest_DbInterfacePool
        UnitTest_ThreadPool
        PROPERTIES LABELS "unit")