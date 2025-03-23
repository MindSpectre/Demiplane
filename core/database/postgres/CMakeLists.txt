##############################################################################
# Postgres Database client implementation used on PQXX lib ONLY FOR C++
##############################################################################
add_library(${PROJECT_NAME}.${DATABASE}.Postgres
        source/pqxx_client.cpp
        include/pqxx_utilities.hpp
        include/pqxx_qeury_processor.hpp
        include/pqxx_client.hpp
        include/pqxx_configurator.hpp
)

target_link_libraries(${PROJECT_NAME}.${DATABASE}.Postgres
        PUBLIC
        Demiplane::Database::Interface
        Demiplane::Database::Exceptions
        Demiplane::Scroll::Tracer
        Demiplane::Stopwatch
        libpqxx::pqxx
        Boost::container
)

target_include_directories(${PROJECT_NAME}.${DATABASE}.Postgres
        PUBLIC
        include
)
add_library(Demiplane::Database::Postgres ALIAS ${PROJECT_NAME}.${DATABASE}.Postgres)
##############################################################################
