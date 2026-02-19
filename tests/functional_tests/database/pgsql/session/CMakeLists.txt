##############################################################################
# PostgreSQL Session Functional Tests
##############################################################################

add_functional_test(
        ${FUNCTIONAL_POSTGRESQL_TESTS}.Session
        session_test.cpp
        LINK_LIBS
        Demiplane::Component::Database::PostgreSQL
        ${TEST_LIBS}
        Boost::asio
        LABELS "pgsql"
        ENVIRONMENT ${PGSQL_TEST_ENV}
)
