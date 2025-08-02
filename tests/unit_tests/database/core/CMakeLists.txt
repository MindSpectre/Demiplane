# Database Core Unit Tests
add_unit_test(Demiplane.Tests.Unit.Database.Core.FieldSchema
        test_field_schema.cpp
        LINK_LIBS
        ${TEST_LIBS}
        Demiplane.Common.Gears
        Demiplane.Component.Database.Core
)

add_unit_test(Demiplane.Tests.Unit.Database.Core.TableSchema
        test_table_schema.cpp
        LINK_LIBS
        ${TEST_LIBS}
        Demiplane.Common.Gears
        Demiplane.Component.Database.Core
)

add_unit_test(Demiplane.Tests.Unit.Database.Core.Column
        test_column.cpp
        LINK_LIBS
        ${TEST_LIBS}
        Demiplane.Common.Gears
        Demiplane.Component.Database.Core
)

add_unit_test(Demiplane.Tests.Unit.Database.Core.Field
        test_field.cpp
        LINK_LIBS
        ${TEST_LIBS}
        Demiplane.Common.Gears
        Demiplane.Component.Database.Core
)