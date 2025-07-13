##############################################################################
#  Http uint test
##############################################################################
add_unit_test(${UNIT_TESTING_TARGET}.Http.Handler
        handler/manual_example_http_handler.cpp
)

target_link_libraries(${UNIT_TESTING_TARGET}.Http.Handler
        Demiplane::Component::Http
        Demiplane::Common::Math
        ${TEST_LIBS}
)
##############################################################################

