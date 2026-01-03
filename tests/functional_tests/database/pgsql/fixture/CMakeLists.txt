# Interface library for shared PostgreSQL test fixture
add_library(${FUNCTIONAL_POSTGRESQL_TESTS}.TestFixture INTERFACE
    test_fixture.hpp
)
target_include_directories(${FUNCTIONAL_POSTGRESQL_TESTS}.TestFixture INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)
