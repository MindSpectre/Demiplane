##############################################################################
# Mock Database client implementation used for testing purposes
##############################################################################
add_library(${PROJECT_NAME}.${DATABASE}.BasicMockClient
        basic_mock/source/basic_mock_db_client.cpp
        basic_mock/include/basic_mock_db_client.hpp
)

target_link_libraries(${PROJECT_NAME}.${DATABASE}.BasicMockClient
        PUBLIC
        Demiplane::Database::Interface
        Demiplane::Scroll::Tracer
)

target_include_directories(${PROJECT_NAME}.${DATABASE}.BasicMockClient
        PUBLIC
        basic_mock/include/
)
add_library(Demiplane::Database::Mock::Basic ALIAS ${PROJECT_NAME}.${DATABASE}.BasicMockClient)
##############################################################################

##############################################################################
# Mock Database client implementation used for testing purposes
##############################################################################
add_library(${PROJECT_NAME}.${DATABASE}.SilentMockClient
        silent_mock/source/silent_mock_db_client.cpp
        silent_mock/include/silent_mock_db_client.hpp
)

target_link_libraries(${PROJECT_NAME}.${DATABASE}.SilentMockClient
        PUBLIC
        Demiplane::Database::Interface
        Demiplane::Utilities
)

target_include_directories(${PROJECT_NAME}.${DATABASE}.SilentMockClient
        PUBLIC
        silent_mock/include/
)
add_library(Demiplane::Database::Mock::Silent ALIAS ${PROJECT_NAME}.${DATABASE}.SilentMockClient)
##############################################################################
