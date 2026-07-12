##############################################################################
# Test HTTP Routing layer
##############################################################################
add_unit_test(${UNIT_TESTING_TARGET}.Http.Routing
        routing/test_route_registry.cpp
        routing/test_controller.cpp
        routing/test_middleware.cpp
        routing/test_group.cpp
        routing/test_router.cpp
        routing/test_routing_allocation_gate.cpp
)
target_link_libraries(${UNIT_TESTING_TARGET}.Http.Routing
        PRIVATE
        Demiplane.Component.HTTP.Routing
        Demiplane.Component.HTTP.Types
        ${TEST_LIBS}
)
##############################################################################

##############################################################################
# Test HTTP Connection layer
##############################################################################
add_unit_test(${UNIT_TESTING_TARGET}.Http.Connection
        connection/test_request_arena.cpp
        connection/test_connection_concepts.cpp
        connection/test_tcp_connection.cpp
        connection/test_tls_connection.cpp
)
target_link_libraries(${UNIT_TESTING_TARGET}.Http.Connection
        PRIVATE
        Demiplane.Component.HTTP.Connection
        Demiplane.Component.HTTP.Types
        Boost::beast
        OpenSSL::SSL
        OpenSSL::Crypto
        ${TEST_LIBS}
)
##############################################################################

##############################################################################
# Test HTTP Types layer
##############################################################################
add_unit_test(${UNIT_TESTING_TARGET}.Http.Types
        types/test_http_enums.cpp
        types/test_url_decode.cpp
        types/test_http_date.cpp
        types/test_headers.cpp
        types/test_body.cpp
        types/test_response.cpp
        types/test_response_factory.cpp
        types/test_errors.cpp
        types/test_request_context.cpp
        types/test_allocation_gate.cpp
)
# Dotted aggregate of all the per-type Http Types leaf targets (no :: alias).
# Deliberately NOT the Demiplane.Component.HTTP umbrella: per-layer links
# catch under-linking regressions in each layer's CMake (PR 7 D4); umbrella
# propagation is guarded by the dedicated ...Http.Umbrella target below.
target_link_libraries(${UNIT_TESTING_TARGET}.Http.Types
        PRIVATE
        Demiplane.Component.HTTP.Types
        ${TEST_LIBS}
)
##############################################################################

##############################################################################
# Test HTTP Drivers layer
##############################################################################
add_unit_test(${UNIT_TESTING_TARGET}.Http.Drivers
        drivers/test_driver_scaffolds.cpp
        drivers/test_driver_helpers.cpp
        drivers/test_http11_driver.cpp
        drivers/test_driver_allocation_gate.cpp
)
target_link_libraries(${UNIT_TESTING_TARGET}.Http.Drivers
        PRIVATE
        Demiplane.Component.HTTP.Drivers
        Demiplane.Component.HTTP.Connection
        Demiplane.Component.HTTP.Routing
        Demiplane.Component.HTTP.Types
        Boost::beast
        ${TEST_LIBS}
)
##############################################################################

##############################################################################
# Test HTTP Listeners layer
##############################################################################
add_unit_test(${UNIT_TESTING_TARGET}.Http.Listeners
        listeners/test_connection_tracker.cpp
        listeners/test_tcp_listener.cpp
        listeners/test_build_ssl_context.cpp
        listeners/test_tls_listener.cpp
        listeners/test_quic_listener.cpp
)
target_link_libraries(${UNIT_TESTING_TARGET}.Http.Listeners
        PRIVATE
        Demiplane.Component.HTTP.Listeners
        Demiplane.Component.HTTP.Connection
        Demiplane.Component.HTTP.Drivers
        Demiplane.Component.HTTP.Routing
        Demiplane.Component.HTTP.Types
        Demiplane.Component.HTTP.Config
        Boost::beast
        OpenSSL::SSL
        OpenSSL::Crypto
        ${TEST_LIBS}
)
target_include_directories(${UNIT_TESTING_TARGET}.Http.Listeners PRIVATE
        ${CMAKE_SOURCE_DIR}/tests/integration_tests/http
)
##############################################################################

##############################################################################
# Test HTTP Config layer
##############################################################################
add_unit_test(${UNIT_TESTING_TARGET}.Http.Config
        config/test_timeouts.cpp
        config/test_tls_config.cpp
        config/test_listener_config.cpp
        config/test_server_config.cpp
        config/test_load_server_config.cpp
)
target_link_libraries(${UNIT_TESTING_TARGET}.Http.Config
        PRIVATE
        Demiplane.Component.HTTP.Config
        Demiplane.Component.HTTP.Types
        JsonCpp::JsonCpp
        ${TEST_LIBS}
)
##############################################################################

##############################################################################
# Test HTTP Server layer (unit — attach_default_listeners; the Server
# lifecycle itself is integration-tested)
##############################################################################
add_unit_test(${UNIT_TESTING_TARGET}.Http.Server
        server/test_attach_default_listeners.cpp
)
target_link_libraries(${UNIT_TESTING_TARGET}.Http.Server
        PRIVATE
        Demiplane.Component.HTTP.Server
        Demiplane.Component.HTTP.Config
        Demiplane.Component.HTTP.Drivers
        Demiplane.Component.HTTP.Types
        Boost::asio
        ${TEST_LIBS}
)
##############################################################################

##############################################################################
# Test the public umbrella (PR 7): <demiplane/http> + the combined
# Demiplane.Component.HTTP library must expose every layer with NO other link.
# This target deliberately links nothing else (gtest aside) — it is the only
# place the consumer-facing contract (one include, one library) is compiled.
# It does NOT isolate per-layer LIBRARIES drops: the layer aggregates
# PUBLIC-link each other, so only .Connection and .Server are load-bearing
# here (see the measured caveat in umbrella/test_umbrella.cpp). Per-layer
# under-linking is guarded by the per-layer dotted links on the targets above (D4).
##############################################################################
add_unit_test(${UNIT_TESTING_TARGET}.Http.Umbrella
        umbrella/test_umbrella.cpp
)
target_link_libraries(${UNIT_TESTING_TARGET}.Http.Umbrella
        PRIVATE
        Demiplane.Component.HTTP
        ${TEST_LIBS}
)
##############################################################################
