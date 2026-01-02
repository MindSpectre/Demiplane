add_library(${TESTS_SHARED}.Database.QueryLibrary STATIC
        include/test_schemas.hpp
        include/query_library.hpp
        include/query_tags.hpp
        include/query_producer.hpp
        include/producers/aggregate_producers.hpp
        include/producers/clause_producers.hpp
        include/producers/condition_producers.hpp
        include/producers/delete_producers.hpp
        include/producers/insert_producers.hpp
        include/producers/join_producers.hpp
        include/producers/select_producers.hpp
        include/producers/subquery_producers.hpp
        include/producers/update_producers.hpp
        src/test_schemas.cpp
)

target_include_directories(${TESTS_SHARED}.Database.QueryLibrary PUBLIC
        include/
)

target_link_libraries(${TESTS_SHARED}.Database.QueryLibrary
        PUBLIC
        Demiplane::Component::Database::PostgreSQL
)


