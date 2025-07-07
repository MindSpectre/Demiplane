##############################################################################
#  Nexus uint test
##############################################################################
add_unit_test(${UNIT_TESTING_TARGET}.Http.Handler
        handler/test_handler.cpp
)

target_link_libraries(${UNIT_TESTING_TARGET}.Http.Handler
        Demiplane::Component::Http
        ${TEST_LIBS}
)
##############################################################################