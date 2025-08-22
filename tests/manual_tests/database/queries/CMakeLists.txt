set(DB_Query_Man_Check Database.Queries)
set(Query_LINK_LIBS
        Demiplane.Component.Database.Query.Compiler
        Demiplane.Component.Database.Dialect.PostgreSQL
        Demiplane::Common::Scroll
        Demiplane::Common::Nexus
)
# Comprehensive query expression tests split into focused files
add_custom_test(${MANUAL_TESTING_TARGET}.${DB_Query_Man_Check}.SelectTest
        db_select_queries_test.cpp
        LINK_LIBS
        ${TEST_LIBS}
        ${Query_LINK_LIBS}
        LABEL DB_Query_Man_Check
)

add_custom_test(${MANUAL_TESTING_TARGET}.${DB_Query_Man_Check}.InsertTest
        db_insert_queries_test.cpp
        LINK_LIBS
        ${TEST_LIBS}
        ${Query_LINK_LIBS}
        LABEL DB_Query_Man_Check
)

add_custom_test(${MANUAL_TESTING_TARGET}.${DB_Query_Man_Check}.UpdateTest
        db_update_queries_test.cpp
        LINK_LIBS
        ${TEST_LIBS}
        ${Query_LINK_LIBS}
        LABEL DB_Query_Man_Check
)

add_custom_test(${MANUAL_TESTING_TARGET}.${DB_Query_Man_Check}.DeleteTest
        db_delete_queries_test.cpp
        LINK_LIBS
        ${TEST_LIBS}
        ${Query_LINK_LIBS}
        LABEL DB_Query_Man_Check
)

add_custom_test(${MANUAL_TESTING_TARGET}.${DB_Query_Man_Check}.AggregateTest
        db_aggregate_queries_test.cpp
        LINK_LIBS
        ${TEST_LIBS}
        ${Query_LINK_LIBS}
        LABEL DB_Query_Man_Check
)

add_custom_test(${MANUAL_TESTING_TARGET}.${DB_Query_Man_Check}.ConditionTest
        db_condition_queries_test.cpp
        LINK_LIBS
        ${TEST_LIBS}
        ${Query_LINK_LIBS}
        LABEL DB_Query_Man_Check
)

# Additional comprehensive query expression tests
add_custom_test(${MANUAL_TESTING_TARGET}.${DB_Query_Man_Check}.JoinTest
        db_join_queries_test.cpp
        LINK_LIBS
        ${TEST_LIBS}
        ${Query_LINK_LIBS}
        LABEL DB_Query_Man_Check
)

add_custom_test(${MANUAL_TESTING_TARGET}.${DB_Query_Man_Check}.SubqueryTest
        db_subquery_queries_test.cpp
        LINK_LIBS
        ${TEST_LIBS}
        ${Query_LINK_LIBS}
        LABEL DB_Query_Man_Check
)

add_custom_test(${MANUAL_TESTING_TARGET}.${DB_Query_Man_Check}.CaseTest
        db_case_queries_test.cpp
        LINK_LIBS
        ${TEST_LIBS}
        ${Query_LINK_LIBS}
        LABEL DB_Query_Man_Check
)

add_custom_test(${MANUAL_TESTING_TARGET}.${DB_Query_Man_Check}.SetOperationsTest
        db_set_operations_test.cpp
        LINK_LIBS
        ${TEST_LIBS}
        ${Query_LINK_LIBS}
        LABEL DB_Query_Man_Check
)

add_custom_test(${MANUAL_TESTING_TARGET}.${DB_Query_Man_Check}.CteTest
        db_cte_queries_test.cpp
        LINK_LIBS
        ${TEST_LIBS}
        ${Query_LINK_LIBS}
        LABEL DB_Query_Man_Check
)

add_custom_test(${MANUAL_TESTING_TARGET}.${DB_Query_Man_Check}.ClauseTest
        db_clause_queries_test.cpp
        LINK_LIBS
        ${TEST_LIBS}
        ${Query_LINK_LIBS}
        LABEL DB_Query_Man_Check
)