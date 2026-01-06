##############################################################################
# PostgreSQL Compiled Query Functional Tests
# Tests query compilation + execution with SyncExecutor
##############################################################################

# Common link libraries for all query tests
set(PGSQL_QUERY_TEST_LIBS
        Demiplane::Component::Database::PostgreSQL
        ${TESTS_SHARED}.Database.QueryLibrary
        ${FUNCTIONAL_POSTGRESQL_TESTS}.TestFixture
        ${TEST_LIBS}
)

# SELECT query tests (using QueryLibrary)
add_functional_test(
        ${FUNCTIONAL_POSTGRESQL_TESTS}.Query.CompiledSelect
        compiled_select_test.cpp
        LINK_LIBS
        ${PGSQL_QUERY_TEST_LIBS}
        LABELS "pgsql"
        ENVIRONMENT ${PGSQL_TEST_ENV}
)

# INSERT query tests (using QueryLibrary)
add_functional_test(
        ${FUNCTIONAL_POSTGRESQL_TESTS}.Query.CompiledInsert
        compiled_insert_test.cpp
        LINK_LIBS ${PGSQL_QUERY_TEST_LIBS}
        LABELS "pgsql"
        ENVIRONMENT ${PGSQL_TEST_ENV}
)

# UPDATE query tests (using QueryLibrary)
add_functional_test(
        ${FUNCTIONAL_POSTGRESQL_TESTS}.Query.CompiledUpdate
        compiled_update_test.cpp
        LINK_LIBS ${PGSQL_QUERY_TEST_LIBS}
        LABELS "pgsql"
        ENVIRONMENT ${PGSQL_TEST_ENV}
)

# DELETE query tests (using QueryLibrary)
add_functional_test(
        ${FUNCTIONAL_POSTGRESQL_TESTS}.Query.CompiledDelete
        compiled_delete_test.cpp
        LINK_LIBS ${PGSQL_QUERY_TEST_LIBS}
        LABELS "pgsql"
        ENVIRONMENT ${PGSQL_TEST_ENV}
)

# JOIN query tests (using QueryLibrary)
add_functional_test(
        ${FUNCTIONAL_POSTGRESQL_TESTS}.Query.CompiledJoin
        compiled_join_test.cpp
        LINK_LIBS ${PGSQL_QUERY_TEST_LIBS}
        LABELS "pgsql"
        ENVIRONMENT ${PGSQL_TEST_ENV}
)

# AGGREGATE query tests (using QueryLibrary)
add_functional_test(
        ${FUNCTIONAL_POSTGRESQL_TESTS}.Query.CompiledAggregate
        compiled_aggregate_test.cpp
        LINK_LIBS ${PGSQL_QUERY_TEST_LIBS}
        LABELS "pgsql"
        ENVIRONMENT ${PGSQL_TEST_ENV}
)

# SUBQUERY query tests (using QueryLibrary)
add_functional_test(
        ${FUNCTIONAL_POSTGRESQL_TESTS}.Query.CompiledSubquery
        compiled_subquery_test.cpp
        LINK_LIBS ${PGSQL_QUERY_TEST_LIBS}
        LABELS "pgsql"
        ENVIRONMENT ${PGSQL_TEST_ENV}
)

# CONDITION query tests (using QueryLibrary)
add_functional_test(
        ${FUNCTIONAL_POSTGRESQL_TESTS}.Query.CompiledCondition
        compiled_condition_test.cpp
        LINK_LIBS ${PGSQL_QUERY_TEST_LIBS}
        LABELS "pgsql"
        ENVIRONMENT ${PGSQL_TEST_ENV}
)

# CLAUSE query tests (using QueryLibrary)
add_functional_test(
        ${FUNCTIONAL_POSTGRESQL_TESTS}.Query.CompiledClause
        compiled_clause_test.cpp
        LINK_LIBS ${PGSQL_QUERY_TEST_LIBS}
        LABELS "pgsql"
        ENVIRONMENT ${PGSQL_TEST_ENV}
)

# CTE (Common Table Expression) query tests (using QueryLibrary)
add_functional_test(
        ${FUNCTIONAL_POSTGRESQL_TESTS}.Query.CompiledCte
        compiled_cte_test.cpp
        LINK_LIBS ${PGSQL_QUERY_TEST_LIBS}
        LABELS "pgsql"
        ENVIRONMENT ${PGSQL_TEST_ENV}
)

# CASE expression query tests (using QueryLibrary)
add_functional_test(
        ${FUNCTIONAL_POSTGRESQL_TESTS}.Query.CompiledCase
        compiled_case_test.cpp
        LINK_LIBS ${PGSQL_QUERY_TEST_LIBS}
        LABELS "pgsql"
        ENVIRONMENT ${PGSQL_TEST_ENV}
)

# SET operations query tests (using QueryLibrary)
add_functional_test(
        ${FUNCTIONAL_POSTGRESQL_TESTS}.Query.CompiledSetOperations
        compiled_set_operations_test.cpp
        LINK_LIBS ${PGSQL_QUERY_TEST_LIBS}
        LABELS "pgsql"
        ENVIRONMENT ${PGSQL_TEST_ENV}
)

# DDL query tests (CREATE TABLE, DROP TABLE)
add_functional_test(
        ${FUNCTIONAL_POSTGRESQL_TESTS}.Query.CompiledDdl
        compiled_ddl_test.cpp
        LINK_LIBS ${PGSQL_QUERY_TEST_LIBS}
        LABELS "pgsql"
        ENVIRONMENT ${PGSQL_TEST_ENV}
)
