set(UNIT_TESTING_TARGET ${PROJECT_NAME}.Tests.Unit)
set(FUNCTIONAL_TESTING_TARGET ${PROJECT_NAME}.Tests.Functional)
set(INTEGRATION_TESTING_TARGET ${PROJECT_NAME}.Tests.Integration)
set(TEST_NECESSARY_LIBS
        GTest::gtest
        GTest::gtest_main
        GTest::gmock
        GTest::gmock_main
        Demiplane::Utilities
        Demiplane::Stopwatch
        Demiplane::Scroll::Tracer
)


add_executable(${PROJECT_NAME}.Tests.Sandbox
        sandbox/main.cpp
)
target_link_libraries(${PROJECT_NAME}.Tests.Sandbox
        PRIVATE
        Demiplane::Scroll::Tracer
)

add_subdirectory(unit_tests)
add_subdirectory(integration_tests)