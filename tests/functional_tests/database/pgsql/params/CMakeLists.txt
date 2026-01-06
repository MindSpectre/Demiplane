##############################################################################
# PostgreSQL Params Functional Tests
##############################################################################

add_functional_test(
        ${FUNCTIONAL_POSTGRESQL_TESTS}.Params
        postgres_params_test.cpp
        LINK_LIBS
        Demiplane::Component::Database::PostgreSQL
        ${TEST_LIBS}
        LABELS "pgsql"
        ENVIRONMENT ${PGSQL_TEST_ENV}
)
