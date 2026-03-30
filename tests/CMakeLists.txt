set(TESTS_TARGET ${PROJECT_NAME}.Tests)
set(UNIT_TESTING_TARGET ${TESTS_TARGET}.Unit)
set(FUNCTIONAL_TESTING_TARGET ${TESTS_TARGET}.Functional)
set(INTEGRATION_TESTING_TARGET ${TESTS_TARGET}.Integration)
set(MANUAL_TESTING_TARGET ${TESTS_TARGET}.Manual)

set(TEST_LIBS
        GTest::gtest
        GTest::gtest_main
        GTest::gmock
        GTest::gmock_main
)

add_subdirectory(unit_tests)

add_subdirectory(integration_tests)

add_subdirectory(functional_tests)

add_subdirectory(codesandbox)
##############################################################################
# Http component test
##############################################################################
if (BUILD_HTTP)
    message("Nexus tests will be built")
    add_subdirectory(manual_tests/http)
endif ()
##############################################################################

##############################################################################
# Base utilities objects for database
##############################################################################
if (BUILD_DATABASE)
    message("Database tests will be built")
    add_subdirectory(unit_tests/database)
    add_subdirectory(integration_tests/database)
endif ()
##############################################################################


