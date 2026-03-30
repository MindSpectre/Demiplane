# Database Query Unit Tests
add_unit_test(${UNIT_TESTING_TARGET}.Database.Query.StaticTableQuery
        test_static_table_query.cpp
        LINK_LIBS
        ${TEST_LIBS}
        Demiplane.Common.Gears
        Demiplane.Component.Database.Primitives
        Demiplane.Component.Database.Query.Compiler
        Demiplane.Component.Database.Providers.PostgreSQL.Dialect
        Demiplane.Component.Database.Providers.PostgreSQL.TypeMapping
)

add_unit_test(${UNIT_TESTING_TARGET}.Database.Query.QueryCompilation
        test_query_compilation.cpp
        LINK_LIBS
        ${TEST_LIBS}
        Demiplane.Common.Gears
        Demiplane.Component.Database.Primitives
        Demiplane.Component.Database.Query.Compiler
        Demiplane.Component.Database.Providers.PostgreSQL.Dialect
        Demiplane.Component.Database.Providers.PostgreSQL.TypeMapping
)
