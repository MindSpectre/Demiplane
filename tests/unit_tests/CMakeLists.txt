##############################################################################
# Test Stopwatch benchmark
##############################################################################
add_unit_test(${UNIT_TESTING_TARGET}.Stopwatch
        stopwatch/test_printing_stopwatch.cpp
        stopwatch/test_stopwatch.cpp
        stopwatch/main.cpp
)
target_link_libraries(${UNIT_TESTING_TARGET}.Stopwatch
        PRIVATE
        Demiplane::Common::Chrono
        ${TEST_LIBS}
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
        Demiplane::Common::Multithread
        ${TEST_LIBS}

)
##############################################################################

##############################################################################
# Test scroll
##############################################################################
add_unit_test(${UNIT_TESTING_TARGET}.Scroll
        scroll/logger_provider_test.cpp
        scroll/entry_tests.cpp
        scroll/main.cpp
        scroll/logger/file_logger_test.cpp
        scroll/logger/console_logger_test.cpp
)
target_link_libraries(${UNIT_TESTING_TARGET}.Scroll
        PRIVATE
        Demiplane::Common::Scroll
        ${TEST_LIBS}
)
##############################################################################



