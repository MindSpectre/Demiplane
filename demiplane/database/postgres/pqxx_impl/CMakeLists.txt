##############################################################################
# Postgres Database client implementation used on PQXX lib ONLY FOR C++
##############################################################################
add_library(${PROJECT_NAME}_Database_PqxxClient
        source/pqxx_client.cpp
        include/pqxx_utilities.hpp
        include/pqxx_controller.hpp
        include/pqxx_client.hpp
)

target_link_libraries(${PROJECT_NAME}_Database_PqxxClient
        PUBLIC
        Demiplane::Database::Interface
        Demiplane::Database::Exceptions
        Demiplane::Stopwatch::Stopwatch
        Demiplane::Database::Strategies
        libpqxx::pqxx
        Boost::container
)

target_include_directories(${PROJECT_NAME}_Database_PqxxClient
        PUBLIC
        include/
)
add_library(Demiplane::Database::Postgres ALIAS ${PROJECT_NAME}_Database_PqxxClient)
##############################################################################
