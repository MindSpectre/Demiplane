##############################################################################
#  Nexus uint test
##############################################################################
add_executable(${UNIT_TESTING_TARGET}.Nexus
        test_nexus.cpp
)

target_link_libraries(${UNIT_TESTING_TARGET}.Nexus
        Demiplane::Component::Nexus
        ${TEST_LIBS}
)
##############################################################################