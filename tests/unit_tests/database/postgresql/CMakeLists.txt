# PostgreSQL Unit Tests
# TODO: Add transaction_status_test.cpp — unit tests for TransactionStatus state transitions
#       without a database (pure logic tests).
add_unit_test(${UNIT_TESTING_TARGET}.Database.PostgreSQL.TransactionOptions
        transaction_options_test.cpp
        LINK_LIBS
        ${TEST_LIBS}
        Demiplane.Component.Database.Providers.PostgreSQL.Transaction
        LABELS "pgsql"
)
