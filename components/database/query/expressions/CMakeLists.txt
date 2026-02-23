##############################################################################
# Basic SQL expressions for static query building
##############################################################################
add_library(${DMP_DATABASE}.Query.Expressions INTERFACE
        query_expressions.hpp
        basic.hpp
        db_expressions_fwd.hpp
        definitions/aggregate_exp.hpp
        definitions/between_exp.hpp
        definitions/case_exp.hpp
        definitions/condition_exp.hpp
        definitions/create_table_exp.hpp
        definitions/cte_exp.hpp
        definitions/delete_exp.hpp
        definitions/delete_where_exp.hpp
        definitions/drop_table_exp.hpp
        definitions/exist_exp.hpp
        definitions/from_exp.hpp
        definitions/group_by_exp.hpp
        definitions/having_exp.hpp
        definitions/in_list_exp.hpp
        definitions/insert_exp.hpp
        definitions/join_exp.hpp
        definitions/limit_exp.hpp
        definitions/order_by_exp.hpp
        definitions/select_exp.hpp
        definitions/set_op_exp.hpp
        definitions/subquery_exp.hpp
        definitions/update_exp.hpp
        definitions/update_where_exp.hpp
        definitions/where_exp.hpp
)
target_include_directories(${DMP_DATABASE}.Query.Expressions INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(${DMP_DATABASE}.Query.Expressions INTERFACE
        ${DMP_DATABASE}.Primitives
)
##############################################################################