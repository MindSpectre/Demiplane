##############################################################################
# Mock Database client implementation used for testing purposes
##############################################################################
add_library(${DMP_DATABASE}.BasicMockClient STATIC
        basic_mock/source/basic_mock_db_client.cpp
        basic_mock/include/basic_mock_db_client.hpp
)

target_link_libraries(${DMP_DATABASE}.BasicMockClient
        PUBLIC
        ${DMP_DATABASE}.Base
)

target_include_directories(${DMP_DATABASE}.BasicMockClient
        PUBLIC
        basic_mock/include
)
add_library(Demiplane::Database::Mock::Basic ALIAS ${DMP_DATABASE}.BasicMockClient)
##############################################################################

##############################################################################
# Mock Database client implementation used for testing purposes
##############################################################################
add_library(${DMP_DATABASE}.SilentMockClient STATIC
        silent_mock/source/silent_mock_db_client.cpp
)

target_link_libraries(${DMP_DATABASE}.SilentMockClient
        PUBLIC
        Demiplane::Math
        ${DMP_DATABASE}.Base
)

target_include_directories(${DMP_DATABASE}.SilentMockClient
        PUBLIC
        silent_mock/include
)
add_library(Demiplane::Database::Mock::Silent ALIAS ${DMP_DATABASE}.SilentMockClient)
##############################################################################
