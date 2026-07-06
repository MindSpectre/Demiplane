##############################################################################
# Http integration tests — real 127.0.0.1:0 sockets via Boost.Beast clients.
##############################################################################
add_integration_test(${INTEGRATION_TESTING_TARGET}.Http.Tcp
        test_http_tcp.cpp
        test_http_drain.cpp
        LINK_LIBS
        Demiplane.Component.HTTP.Listeners
        Demiplane.Component.HTTP.Drivers
        Demiplane.Component.HTTP.Routing
        Demiplane.Component.HTTP.Connection
        Demiplane.Component.HTTP.Types
        Demiplane.Component.HTTP.Config
        Boost::beast
        OpenSSL::SSL
        OpenSSL::Crypto
        ${TEST_LIBS}
        LABELS "http"
)
target_include_directories(${INTEGRATION_TESTING_TARGET}.Http.Tcp PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)
##############################################################################
add_integration_test(${INTEGRATION_TESTING_TARGET}.Http.Tls
        test_http_tls.cpp
        LINK_LIBS
        Demiplane.Component.HTTP.Listeners
        Demiplane.Component.HTTP.Drivers
        Demiplane.Component.HTTP.Routing
        Demiplane.Component.HTTP.Connection
        Demiplane.Component.HTTP.Types
        Demiplane.Component.HTTP.Config
        Boost::beast
        OpenSSL::SSL
        OpenSSL::Crypto
        ${TEST_LIBS}
        LABELS "http"
)
target_include_directories(${INTEGRATION_TESTING_TARGET}.Http.Tls PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)
##############################################################################
add_integration_test(${INTEGRATION_TESTING_TARGET}.Http.Server
        test_http_server_lifecycle.cpp
        test_http_server_observer.cpp
        test_http_run_standalone.cpp
        test_http_server_concurrency.cpp
        LINK_LIBS
        Demiplane.Component.HTTP.Server
        Demiplane.Component.HTTP.Listeners
        Demiplane.Component.HTTP.Drivers
        Demiplane.Component.HTTP.Routing
        Demiplane.Component.HTTP.Connection
        Demiplane.Component.HTTP.Types
        Demiplane.Component.HTTP.Config
        Demiplane::Common::Chrono
        Boost::beast
        OpenSSL::SSL
        OpenSSL::Crypto
        ${TEST_LIBS}
        LABELS "http"
)
target_include_directories(${INTEGRATION_TESTING_TARGET}.Http.Server PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)
##############################################################################
