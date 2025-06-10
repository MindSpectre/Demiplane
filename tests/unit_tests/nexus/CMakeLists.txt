##############################################################################
#  Nexus uint test
##############################################################################
add_unit_test(${UNIT_TESTING_TARGET}.Nexus
        test_nexus.cpp
)

target_link_libraries(${UNIT_TESTING_TARGET}.Nexus
        Demiplane::Component::Nexus
        ${TEST_LIBS}
)
##############################################################################