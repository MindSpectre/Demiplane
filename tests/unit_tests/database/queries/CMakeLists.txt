#add_unit_test(${UNIT_TESTING_TARGET}.Database.Queries.BuildTest
#        db_queries_validation.cpp
#        LINK_LIBS
#        ${TEST_LIBS}
#        Demiplane.Component.Database.Query.Compiler
#        Demiplane.Component.Database.Dialect.PostgreSQL
#)

# Comprehensive query expression tests split into focused files
add_unit_test(${UNIT_TESTING_TARGET}.Database.Queries.SelectTest
        db_select_queries_test.cpp
        LINK_LIBS
        ${TEST_LIBS}
        Demiplane.Component.Database.Query.Compiler
        Demiplane.Component.Database.Dialect.PostgreSQL
)

add_unit_test(${UNIT_TESTING_TARGET}.Database.Queries.InsertTest
        db_insert_queries_test.cpp
        LINK_LIBS
        ${TEST_LIBS}
        Demiplane.Component.Database.Query.Compiler
        Demiplane.Component.Database.Dialect.PostgreSQL
)

add_unit_test(${UNIT_TESTING_TARGET}.Database.Queries.UpdateTest
        db_update_queries_test.cpp
        LINK_LIBS
        ${TEST_LIBS}
        Demiplane.Component.Database.Query.Compiler
        Demiplane.Component.Database.Dialect.PostgreSQL
)

add_unit_test(${UNIT_TESTING_TARGET}.Database.Queries.DeleteTest
        db_delete_queries_test.cpp
        LINK_LIBS
        ${TEST_LIBS}
        Demiplane.Component.Database.Query.Compiler
        Demiplane.Component.Database.Dialect.PostgreSQL
)

add_unit_test(${UNIT_TESTING_TARGET}.Database.Queries.AggregateTest
        db_aggregate_queries_test.cpp
        LINK_LIBS
        ${TEST_LIBS}
        Demiplane.Component.Database.Query.Compiler
        Demiplane.Component.Database.Dialect.PostgreSQL
)

add_unit_test(${UNIT_TESTING_TARGET}.Database.Queries.ConditionTest
        db_condition_queries_test.cpp
        LINK_LIBS
        ${TEST_LIBS}
        Demiplane.Component.Database.Query.Compiler
        Demiplane.Component.Database.Dialect.PostgreSQL
)

# Additional comprehensive query expression tests
add_unit_test(${UNIT_TESTING_TARGET}.Database.Queries.JoinTest
        db_join_queries_test.cpp
        LINK_LIBS
        ${TEST_LIBS}
        Demiplane.Component.Database.Query.Compiler
        Demiplane.Component.Database.Dialect.PostgreSQL
)

add_unit_test(${UNIT_TESTING_TARGET}.Database.Queries.SubqueryTest
        db_subquery_queries_test.cpp
        LINK_LIBS
        ${TEST_LIBS}
        Demiplane.Component.Database.Query.Compiler
        Demiplane.Component.Database.Dialect.PostgreSQL
)

add_unit_test(${UNIT_TESTING_TARGET}.Database.Queries.CaseTest
        db_case_queries_test.cpp
        LINK_LIBS
        ${TEST_LIBS}
        Demiplane.Component.Database.Query.Compiler
        Demiplane.Component.Database.Dialect.PostgreSQL
)

add_unit_test(${UNIT_TESTING_TARGET}.Database.Queries.SetOperationsTest
        db_set_operations_test.cpp
        LINK_LIBS
        ${TEST_LIBS}
        Demiplane.Component.Database.Query.Compiler
        Demiplane.Component.Database.Dialect.PostgreSQL
)

add_unit_test(${UNIT_TESTING_TARGET}.Database.Queries.CteTest
        db_cte_queries_test.cpp
        LINK_LIBS
        ${TEST_LIBS}
        Demiplane.Component.Database.Query.Compiler
        Demiplane.Component.Database.Dialect.PostgreSQL
)

add_unit_test(${UNIT_TESTING_TARGET}.Database.Queries.ClauseTest
        db_clause_queries_test.cpp
        LINK_LIBS
        ${TEST_LIBS}
        Demiplane.Component.Database.Query.Compiler
        Demiplane.Component.Database.Dialect.PostgreSQL
)