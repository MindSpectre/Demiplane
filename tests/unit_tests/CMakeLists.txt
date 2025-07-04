

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

add_subdirectory(scroll)

##############################################################################
# Base utilities objects for database
##############################################################################
if (BUILD_DATABASE)
    add_subdirectory(database)
endif ()
##############################################################################

##############################################################################
# Nexus component test
##############################################################################
if (BUILD_NEXUS)
    add_subdirectory(nexus)
endif ()
##############################################################################