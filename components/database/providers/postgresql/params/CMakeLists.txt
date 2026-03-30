##############################################################################
# PostgreSql Params
##############################################################################
add_library(${DMP_DATABASE_POSTGRESQL}.Params STATIC
        postgres_params.hpp
        postgres_params.cpp
)
target_include_directories(${DMP_DATABASE_POSTGRESQL}.Params PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(${DMP_DATABASE_POSTGRESQL}.Params
        PUBLIC
        ${DMP_DATABASE}.Base.Params
        ${DMP_DATABASE_POSTGRESQL}.Registries
)
##############################################################################