##############################################################################
# Postgres integration test
##############################################################################
if (BUILD_DATABASE)
    add_integration_test(${INTEGRATION_TESTING_TARGET}.PqxxClientTest
            pqxx_db_client/pqxx_db_client_test.cpp
    )
    target_link_libraries(${INTEGRATION_TESTING_TARGET}.PqxxClientTest
            PRIVATE
            Demiplane::Database
            ${TEST_LIBS}

    )
endif ()
##############################################################################