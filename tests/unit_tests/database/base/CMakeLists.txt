# Database Base Unit Tests
add_unit_test(${UNIT_TESTING_TARGET}.Database.Base.TypeMapping
        test_type_mapping.cpp
        LINK_LIBS
        ${TEST_LIBS}
        Demiplane.Common.Gears
        Demiplane.Component.Database.Primitives
        Demiplane.Component.Database.Providers.PostgreSQL.Dialect
)
