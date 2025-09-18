# Database Core Unit Tests
add_unit_test(${UNIT_TESTING_TARGET}.Database.ORM.Core.FieldSchema
        test_field_schema.cpp
        LINK_LIBS
        ${TEST_LIBS}
        Demiplane.Common.Gears
        Demiplane.Component.Database.ORM.Core
)

add_unit_test(${UNIT_TESTING_TARGET}.Database.ORM.Core.Table
        test_table_schema.cpp
        LINK_LIBS
        ${TEST_LIBS}
        Demiplane.Common.Gears
        Demiplane.Component.Database.ORM.Core
)

add_unit_test(${UNIT_TESTING_TARGET}.Database.ORM.Core.Column
        test_column.cpp
        LINK_LIBS
        ${TEST_LIBS}
        Demiplane.Common.Gears
        Demiplane.Component.Database.ORM.Core
)

add_unit_test(${UNIT_TESTING_TARGET}.Database.ORM.Core.Field
        test_field.cpp
        LINK_LIBS
        ${TEST_LIBS}
        Demiplane.Common.Gears
        Demiplane.Component.Database.ORM.Core
)