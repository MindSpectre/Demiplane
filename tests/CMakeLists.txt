set(UNIT_TESTING_TARGET ${PROJECT_NAME}.Tests.Unit)
set(FUNCTIONAL_TESTING_TARGET ${PROJECT_NAME}.Tests.Functional)
set(INTEGRATION_TESTING_TARGET ${PROJECT_NAME}.Tests.Integration)
set(TEST_NECESSARY_LIBS
        GTest::gtest
        GTest::gtest_main
        GTest::gmock
        GTest::gmock_main
        Demiplane::Math
        Demiplane::Crypto
        Demiplane::Chrono
        Demiplane::Scroll::Tracer
)


add_executable(${PROJECT_NAME}.Tests.Sandbox
        codesandbox/main.cpp
)
target_link_libraries(${PROJECT_NAME}.Tests.Sandbox
        PRIVATE
        Demiplane::Component::Nexus
)

#add_subdirectory(unit_tests)
#add_subdirectory(integration_tests)