set(UNIT_TESTING_TARGET ${PROJECT_NAME}.Tests.Unit)
set(FUNCTIONAL_TESTING_TARGET ${PROJECT_NAME}.Tests.Functional)
set(INTEGRATION_TESTING_TARGET ${PROJECT_NAME}.Tests.Integration)
set(TEST_LIBS
        GTest::gtest
        GTest::gtest_main
        GTest::gmock
        GTest::gmock_main
)

add_subdirectory(examples)

add_subdirectory(unit_tests)

add_subdirectory(integration_tests)


##############################################################################
# Http component test
##############################################################################
if (BUILD_HTTP)
    add_subdirectory(examples/http)
endif ()
##############################################################################

##############################################################################
# Base utilities objects for database
##############################################################################
if (BUILD_DATABASE)
    add_subdirectory(unit_tests/database)
endif ()
##############################################################################

##############################################################################
# Nexus component test
##############################################################################
if (BUILD_NEXUS)
    add_subdirectory(unit_tests/nexus)
endif ()
##############################################################################


add_executable(${PROJECT_NAME}.Tests.Sandbox
        codesandbox/main.cpp
)
target_link_libraries(${PROJECT_NAME}.Tests.Sandbox
        PRIVATE
        Demiplane::Common::Gears
)