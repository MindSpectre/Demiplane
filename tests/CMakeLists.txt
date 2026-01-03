set(TESTS_SHARED ${PROJECT_NAME}.Tests.Shared)
set(UNIT_TESTING_TARGET ${PROJECT_NAME}.Tests.Unit)
set(FUNCTIONAL_TESTING_TARGET ${PROJECT_NAME}.Tests.Functional)
set(INTEGRATION_TESTING_TARGET ${PROJECT_NAME}.Tests.Integration)
set(MANUAL_TESTING_TARGET ${PROJECT_NAME}.Tests.Manual)

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
    add_subdirectory(shared/database)
    add_subdirectory(unit_tests/database)
    add_subdirectory(functional_tests/database)
endif ()
##############################################################################


