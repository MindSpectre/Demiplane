##############################################################################
# PostgreSQL AsyncExecutor Functional Tests
##############################################################################

add_functional_test(
        ${FUNCTIONAL_POSTGRESQL_TESTS}.Capabilities.Executors.Executor.Async
        postgres_async_executor_test.cpp
        LINK_LIBS
        Demiplane::Component::Database::PostgreSQL
        ${TEST_LIBS}
        Boost::asio
        LABELS "pgsql"
        ENVIRONMENT ${PGSQL_TEST_ENV}
)