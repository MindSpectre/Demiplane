set(UNIT_TESTING_TARGET ${PROJECT_NAME}_Tests_Unit)
set(FUNCTIONAL_TESTING_TARGET ${PROJECT_NAME}_Tests_Functional)
set(INTEGRATION_TESTING_TARGET ${PROJECT_NAME}_Tests_Integration)
set(TEST_NECESSARY_LIBS
        GTest::gtest
        GTest::gtest_main
        GTest::gmock
        GTest::gmock_main
        Demiplane::Utilities
        Demiplane::Scroll::Tracer
)


add_executable(${PROJECT_NAME}_Tests_Sandbox
        sandbox/main.cpp
)
target_link_libraries(${PROJECT_NAME}_Tests_Sandbox
        PRIVATE
        Demiplane::Scroll::Tracer
        #        Demiplane::Interceptors
)

add_subdirectory(unit_tests)