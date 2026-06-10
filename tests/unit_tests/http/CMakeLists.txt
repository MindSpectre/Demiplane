##############################################################################
# Test HTTP Routing layer
##############################################################################
add_unit_test(${UNIT_TESTING_TARGET}.Http.Routing
        routing/test_route_registry.cpp
        routing/test_controller.cpp
)
target_link_libraries(${UNIT_TESTING_TARGET}.Http.Routing
        PRIVATE
        Demiplane.Component.HTTP.Routing
        Demiplane.Component.HTTP.Types
        ${TEST_LIBS}
)
##############################################################################

##############################################################################
# Test HTTP Types layer
##############################################################################
add_unit_test(${UNIT_TESTING_TARGET}.Http.Types
        types/test_http_enums.cpp
        types/test_url_decode.cpp
        types/test_headers.cpp
        types/test_body.cpp
        types/test_response.cpp
        types/test_response_factory.cpp
        types/test_errors.cpp
        types/test_request_context.cpp
        types/test_allocation_gate.cpp
)
# Dotted aggregate of all the per-type Http Types leaf targets (no :: alias).
# NOT Demiplane::Component::Http — that umbrella aggregates only the old
# Handler lib until PR 7 and would not propagate the Types include dirs.
target_link_libraries(${UNIT_TESTING_TARGET}.Http.Types
        PRIVATE
        Demiplane.Component.HTTP.Types
        ${TEST_LIBS}
)
##############################################################################