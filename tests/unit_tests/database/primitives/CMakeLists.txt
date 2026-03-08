# Database Core Unit Tests
add_unit_test(${UNIT_TESTING_TARGET}.Database.Primitives.FieldSchema
        test_field_schema.cpp
        LINK_LIBS
        ${TEST_LIBS}
        Demiplane.Common.Gears
        Demiplane.Component.Database.Primitives
)

add_unit_test(${UNIT_TESTING_TARGET}.Database.Primitives.Table
        test_table_schema.cpp
        LINK_LIBS
        ${TEST_LIBS}
        Demiplane.Common.Gears
        Demiplane.Component.Database.Primitives
)

add_unit_test(${UNIT_TESTING_TARGET}.Database.Primitives.Column
        test_column.cpp
        LINK_LIBS
        ${TEST_LIBS}
        Demiplane.Common.Gears
        Demiplane.Component.Database.Primitives
)

add_unit_test(${UNIT_TESTING_TARGET}.Database.Primitives.Field
        test_field.cpp
        LINK_LIBS
        ${TEST_LIBS}
        Demiplane.Common.Gears
        Demiplane.Component.Database.Primitives
)

add_unit_test(${UNIT_TESTING_TARGET}.Database.Primitives.StaticTable
        test_static_table.cpp
        LINK_LIBS
        ${TEST_LIBS}
        Demiplane.Common.Gears
        Demiplane.Component.Database.Primitives
)

add_unit_test(${UNIT_TESTING_TARGET}.Database.Primitives.StaticFieldSchema
        test_static_field_schema.cpp
        LINK_LIBS
        ${TEST_LIBS}
        Demiplane.Common.Gears
        Demiplane.Component.Database.Primitives
)

add_unit_test(${UNIT_TESTING_TARGET}.Database.Primitives.StaticColumn
        test_static_column.cpp
        LINK_LIBS
        ${TEST_LIBS}
        Demiplane.Common.Gears
        Demiplane.Component.Database.Primitives
)