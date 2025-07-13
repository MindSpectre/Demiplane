##############################################################################
#  Http uint test
##############################################################################
add_executable(${MANUAL_TESTING_TARGET}.Http.Handler
        handler/manual_example_http_handler.cpp
)

target_link_libraries(${MANUAL_TESTING_TARGET}.Http.Handler
        Demiplane::Component::Http
        Demiplane::Common::Math
        ${TEST_LIBS}
)
##############################################################################

