##############################################################################
# PostgreSQL Transaction Integration Tests
##############################################################################

add_integration_test(
        ${INTEGRATION_POSTGRESQL_TESTS}.Transaction.Lifecycle
        transaction_lifecycle_test.cpp
        LINK_LIBS
        Demiplane::Component::Database::PostgreSQL
        ${TEST_LIBS}
        Boost::asio
        LABELS "pgsql"
        ENVIRONMENT ${PGSQL_TEST_ENV}
)

add_integration_test(
        ${INTEGRATION_POSTGRESQL_TESTS}.Transaction.Savepoint
        savepoint_test.cpp
        LINK_LIBS
        Demiplane::Component::Database::PostgreSQL
        ${TEST_LIBS}
        Boost::asio
        LABELS "pgsql"
        ENVIRONMENT ${PGSQL_TEST_ENV}
)
