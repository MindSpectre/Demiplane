##############################################################################
#
##############################################################################
add_library(${DMP_DATABASE_POSTGRESQL}.Capabilities.Pipeline STATIC
        postgres_pipeline.cpp
        postgres_pipeline.hpp
)

target_include_directories(${DMP_DATABASE_POSTGRESQL}.Capabilities.Pipeline PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${DMP_DATABASE_POSTGRESQL}.Capabilities.Pipeline PUBLIC

)
##############################################################################