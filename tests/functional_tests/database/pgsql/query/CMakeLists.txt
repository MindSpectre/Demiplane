##############################################################################
# PostgreSQL Compiled Query Functional Tests
# Tests query compilation + execution with SyncExecutor
##############################################################################

# SELECT query tests
add_functional_test(
        ${FUNCTIONAL_TESTING_TARGET}.PostgreSQL.Query.CompiledSelect
        compiled_select_test.cpp
        LINK_LIBS
            Demiplane::Component::Database::PostgreSQL
            ${TEST_LIBS}
        LABELS "pgsql"
        ENVIRONMENT ${PGSQL_TEST_ENV}
)

# INSERT query tests
add_functional_test(
        ${FUNCTIONAL_TESTING_TARGET}.PostgreSQL.Query.CompiledInsert
        compiled_insert_test.cpp
        LINK_LIBS
            Demiplane::Component::Database::PostgreSQL
            ${TEST_LIBS}
        LABELS "pgsql"
        ENVIRONMENT ${PGSQL_TEST_ENV}
)

# UPDATE query tests
add_functional_test(
        ${FUNCTIONAL_TESTING_TARGET}.PostgreSQL.Query.CompiledUpdate
        compiled_update_test.cpp
        LINK_LIBS
            Demiplane::Component::Database::PostgreSQL
            ${TEST_LIBS}
        LABELS "pgsql"
        ENVIRONMENT ${PGSQL_TEST_ENV}
)

# DELETE query tests
add_functional_test(
        ${FUNCTIONAL_TESTING_TARGET}.PostgreSQL.Query.CompiledDelete
        compiled_delete_test.cpp
        LINK_LIBS
            Demiplane::Component::Database::PostgreSQL
            ${TEST_LIBS}
        LABELS "pgsql"
        ENVIRONMENT ${PGSQL_TEST_ENV}
)
