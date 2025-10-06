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
        Demiplane::Common::Math
        Demiplane::Common::Chrono
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


##############################################################################
#  Nexus uint test
##############################################################################
add_unit_test(${UNIT_TESTING_TARGET}.Nexus
        nexus/test_nexus.cpp
)

target_link_libraries(${UNIT_TESTING_TARGET}.Nexus
        Demiplane::Common::Nexus
        ${TEST_LIBS}
)
##############################################################################

##############################################################################
# Test Algorithms
##############################################################################
add_unit_test(${UNIT_TESTING_TARGET}.Algorithms
        algorithms/test_sliding_window_sorter.cpp
)
target_link_libraries(${UNIT_TESTING_TARGET}.Algorithms
        PRIVATE
        Demiplane::Common::Algorithms
        ${TEST_LIBS}
)
##############################################################################

##############################################################################
# Test Gears Result (Outcome)
##############################################################################
add_unit_test(${UNIT_TESTING_TARGET}.Gears.Result
        gears/test_outcome.cpp
)
target_link_libraries(${UNIT_TESTING_TARGET}.Gears.Result
        PRIVATE
        Demiplane::Common::Gears
        ${TEST_LIBS}
)
##############################################################################



