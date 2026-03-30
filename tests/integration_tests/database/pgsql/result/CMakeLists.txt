##############################################################################
# PostgreSQL Result Processing Tests
# Tests FieldView decoders with real PostgreSQL round-trips
##############################################################################

add_integration_test(
        ${INTEGRATION_POSTGRESQL_TESTS}.Result.FieldViewDecoder
        fieldview_decoder_test.cpp
        LINK_LIBS
        Demiplane::Component::Database::PostgreSQL
        ${INTEGRATION_POSTGRESQL_TESTS}.TestFixture
        ${TEST_LIBS}
        LABELS "pgsql"
        ENVIRONMENT ${PGSQL_TEST_ENV}
)
