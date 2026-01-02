add_subdirectory(query_library)

add_combined_library(${TESTS_SHARED}.Database
        LIBRARIES
        ${TESTS_SHARED}.Database.QueryLibrary
)

add_library(Demiplane::Tests::Shared::Database ALIAS ${TESTS_SHARED}.Database)