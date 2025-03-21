##############################################################################
# Mock Database client implementation used for testing purposes
##############################################################################
add_library(${PROJECT_NAME}_Database_SilentMockClient
        source/silent_mock_db_client.cpp
        include/silent_mock_db_client.hpp
)

target_link_libraries(${PROJECT_NAME}_Database_SilentMockClient
        PUBLIC
        Demiplane::Database::Interface
        Demiplane::Database::Exceptions
        Demiplane::Stopwatch
        Demiplane::Utilities
)

target_include_directories(${PROJECT_NAME}_Database_SilentMockClient
        PUBLIC
        include/
)
add_library(Demiplane::Database::Mock::Silent ALIAS ${PROJECT_NAME}_Database_SilentMockClient)
##############################################################################
