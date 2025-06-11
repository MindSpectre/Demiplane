##############################################################################
# Postgres Database client implementation used on PQXX lib ONLY FOR C++
##############################################################################
add_library(${DMP_DATABASE}.Pqxx
        pqxx_query_engine.hpp
        pqxx_configurator.hpp
        source/pqxx_client.cpp
)

target_link_libraries(${DMP_DATABASE}.Pqxx
        PUBLIC
        Boost::container
        libpqxx::pqxx
        Demiplane::Chrono
        ${DMP_DATABASE}.Base
        ${DMP_DATABASE}.Exceptions

)

target_include_directories(${DMP_DATABASE}.Pqxx
        PUBLIC
        include
)
add_library(Demiplane::Database::Pqxx ALIAS ${DMP_DATABASE}.Pqxx)
##############################################################################
