add_unit_test(${UNIT_TESTING_TARGET}.Database.Queries.BuildTest
        db_queries_validation.cpp
        LINK_LIBS
        ${TEST_LIBS}
        Demiplane.Component.Database.Query.Compiler
        Demiplane.Component.Database.Dialect.PostgreSQL
)