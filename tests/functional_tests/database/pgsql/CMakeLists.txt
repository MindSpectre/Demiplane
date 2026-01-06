# Load environment from .env.test.local or .env.test
set(PGSQL_ENV_FILE "${CMAKE_CURRENT_SOURCE_DIR}/.env.test.local")
if (NOT EXISTS "${PGSQL_ENV_FILE}")
    set(PGSQL_ENV_FILE "${CMAKE_CURRENT_SOURCE_DIR}/.env.test")
endif ()

if (EXISTS "${PGSQL_ENV_FILE}")
    file(STRINGS "${PGSQL_ENV_FILE}" ENV_LINES)
    set(PGSQL_TEST_ENV "")
    foreach (LINE ${ENV_LINES})
        if (NOT LINE MATCHES "^#" AND NOT LINE STREQUAL "")
            if (LINE MATCHES "^(POSTGRES_[^=]+)=(.*)$")
                list(APPEND PGSQL_TEST_ENV "${CMAKE_MATCH_1}=${CMAKE_MATCH_2}")
            endif ()
        endif ()
    endforeach ()
endif ()

set(FUNCTIONAL_POSTGRESQL_TESTS ${FUNCTIONAL_TESTING_TARGET}.Database.PostgreSQL)

add_subdirectory(capabilities)
add_subdirectory(params)
add_subdirectory(query)
add_subdirectory(fixture)