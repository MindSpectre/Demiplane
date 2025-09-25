add_library(${DMP_DATABASE_ORM}.Params INTERFACE
        interface/sql_params.hpp
)
target_include_directories(${DMP_DATABASE_ORM}.Params INTERFACE
        interface/
)
target_link_libraries(${DMP_DATABASE_ORM}.Params INTERFACE
        ${DMP_DATABASE_ORM}.Core
)


