add_library(${DMP_DATABASE}.Dialect INTERFACE
        interface/sql_dialect.hpp
)
target_include_directories(${DMP_DATABASE}.Dialect INTERFACE
        interface/
)
target_link_libraries(${DMP_DATABASE}.Dialect INTERFACE
        ${DMP_DATABASE}.ORM.Core
)


