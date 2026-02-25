add_library(${TESTS_SHARED}.Database.QueryLibrary STATIC
        test_schemas.hpp
        query_library.hpp
        query_tags.hpp
        query_producer.hpp
        producers/aggregate_producers.hpp
        producers/case_producers.hpp
        producers/clause_producers.hpp
        producers/condition_producers.hpp
        producers/cte_producers.hpp
        producers/ddl_producers.hpp
        producers/delete_producers.hpp
        producers/insert_producers.hpp
        producers/join_producers.hpp
        producers/select_producers.hpp
        producers/set_op_producers.hpp
        producers/subquery_producers.hpp
        producers/update_producers.hpp
        test_schemas.cpp
)

target_include_directories(${TESTS_SHARED}.Database.QueryLibrary PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${TESTS_SHARED}.Database.QueryLibrary
        PUBLIC
        Demiplane::Component::Database::PostgreSQL
)


