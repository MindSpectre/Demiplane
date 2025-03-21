##############################################################################
# Mock Database client implementation used for testing purposes
##############################################################################
add_library(${PROJECT_NAME}_Database_BasicMockClient
        source/basic_mock_db_client.cpp
        include/basic_mock_db_client.hpp
)

target_link_libraries(${PROJECT_NAME}_Database_BasicMockClient
        PUBLIC
        Demiplane::Database::Interface
        Demiplane::Scroll::Tracer
)

target_include_directories(${PROJECT_NAME}_Database_BasicMockClient
        PUBLIC
        include/
)
add_library(Demiplane::Database::Mock::Basic ALIAS ${PROJECT_NAME}_Database_BasicMockClient)
##############################################################################
