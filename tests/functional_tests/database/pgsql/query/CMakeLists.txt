##############################################################################
# PostgreSQL Compiled Query Functional Tests
# Tests query compilation + execution with SyncExecutor
##############################################################################

# SELECT query tests (using QueryLibrary)
add_functional_test(
        ${FUNCTIONAL_TESTING_TARGET}.PostgreSQL.Query.CompiledSelect
        compiled_select_test.cpp
        LINK_LIBS
            Demiplane::Component::Database::PostgreSQL
            ${TESTS_SHARED}.Database.QueryLibrary
            ${TEST_LIBS}
        LABELS "pgsql"
        ENVIRONMENT ${PGSQL_TEST_ENV}
)

# INSERT query tests (using QueryLibrary)
add_functional_test(
        ${FUNCTIONAL_TESTING_TARGET}.PostgreSQL.Query.CompiledInsert
        compiled_insert_test.cpp
        LINK_LIBS
            Demiplane::Component::Database::PostgreSQL
            ${TESTS_SHARED}.Database.QueryLibrary
            ${TEST_LIBS}
        LABELS "pgsql"
        ENVIRONMENT ${PGSQL_TEST_ENV}
)

# UPDATE query tests (using QueryLibrary)
add_functional_test(
        ${FUNCTIONAL_TESTING_TARGET}.PostgreSQL.Query.CompiledUpdate
        compiled_update_test.cpp
        LINK_LIBS
            Demiplane::Component::Database::PostgreSQL
            ${TESTS_SHARED}.Database.QueryLibrary
            ${TEST_LIBS}
        LABELS "pgsql"
        ENVIRONMENT ${PGSQL_TEST_ENV}
)

# DELETE query tests (using QueryLibrary)
add_functional_test(
        ${FUNCTIONAL_TESTING_TARGET}.PostgreSQL.Query.CompiledDelete
        compiled_delete_test.cpp
        LINK_LIBS
            Demiplane::Component::Database::PostgreSQL
            ${TESTS_SHARED}.Database.QueryLibrary
            ${TEST_LIBS}
        LABELS "pgsql"
        ENVIRONMENT ${PGSQL_TEST_ENV}
)

# JOIN query tests (using QueryLibrary)
add_functional_test(
        ${FUNCTIONAL_TESTING_TARGET}.PostgreSQL.Query.CompiledJoin
        compiled_join_test.cpp
        LINK_LIBS
            Demiplane::Component::Database::PostgreSQL
            ${TESTS_SHARED}.Database.QueryLibrary
            ${TEST_LIBS}
        LABELS "pgsql"
        ENVIRONMENT ${PGSQL_TEST_ENV}
)

# AGGREGATE query tests (using QueryLibrary)
add_functional_test(
        ${FUNCTIONAL_TESTING_TARGET}.PostgreSQL.Query.CompiledAggregate
        compiled_aggregate_test.cpp
        LINK_LIBS
            Demiplane::Component::Database::PostgreSQL
            ${TESTS_SHARED}.Database.QueryLibrary
            ${TEST_LIBS}
        LABELS "pgsql"
        ENVIRONMENT ${PGSQL_TEST_ENV}
)

# SUBQUERY query tests (using QueryLibrary)
add_functional_test(
        ${FUNCTIONAL_TESTING_TARGET}.PostgreSQL.Query.CompiledSubquery
        compiled_subquery_test.cpp
        LINK_LIBS
            Demiplane::Component::Database::PostgreSQL
            ${TESTS_SHARED}.Database.QueryLibrary
            ${TEST_LIBS}
        LABELS "pgsql"
        ENVIRONMENT ${PGSQL_TEST_ENV}
)

# CONDITION query tests (using QueryLibrary)
add_functional_test(
        ${FUNCTIONAL_TESTING_TARGET}.PostgreSQL.Query.CompiledCondition
        compiled_condition_test.cpp
        LINK_LIBS
            Demiplane::Component::Database::PostgreSQL
            ${TESTS_SHARED}.Database.QueryLibrary
            ${TEST_LIBS}
        LABELS "pgsql"
        ENVIRONMENT ${PGSQL_TEST_ENV}
)

# CLAUSE query tests (using QueryLibrary)
add_functional_test(
        ${FUNCTIONAL_TESTING_TARGET}.PostgreSQL.Query.CompiledClause
        compiled_clause_test.cpp
        LINK_LIBS
            Demiplane::Component::Database::PostgreSQL
            ${TESTS_SHARED}.Database.QueryLibrary
            ${TEST_LIBS}
        LABELS "pgsql"
        ENVIRONMENT ${PGSQL_TEST_ENV}
)
