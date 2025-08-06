add_unit_test(${UNIT_TESTING_TARGET}.Database.Queries.Concepts
        test_concept_validation.cpp
        LINK_LIBS
        ${TEST_LIBS}
        Demiplane.Common.Gears
        Demiplane.Component.Database.Core
        Demiplane.Component.Database.Query.Expressions
)