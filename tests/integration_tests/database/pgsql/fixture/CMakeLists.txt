# PostgreSQL test fixture with test schemas
add_library(${INTEGRATION_POSTGRESQL_TESTS}.TestFixture STATIC
        test_fixture.hpp
        test_schemas.hpp
        test_schemas.cpp
)
target_include_directories(${INTEGRATION_POSTGRESQL_TESTS}.TestFixture PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(${INTEGRATION_POSTGRESQL_TESTS}.TestFixture
        PUBLIC
        Demiplane::Component::Database::PostgreSQL
        ${TEST_LIBS}
)
