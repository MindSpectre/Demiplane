##############################################################################
# PostgreSql Params
##############################################################################
add_library(${DMP_DATABASE_POSTGRESQL}.Params STATIC
        include/postgres_params.hpp
        source/postgres_params.cpp
)
target_include_directories(${DMP_DATABASE_POSTGRESQL}.Params PUBLIC
        include
)
target_link_libraries(${DMP_DATABASE_POSTGRESQL}.Params
        PUBLIC
        ${DMP_DATABASE}.Base.Params
        ${DMP_DATABASE_POSTGRESQL}.Shared
)
##############################################################################