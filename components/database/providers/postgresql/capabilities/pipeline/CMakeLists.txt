##############################################################################
#
##############################################################################
add_library(${DMP_DATABASE_POSTGRESQL}.Capabilities.Pipeline STATIC
        source/postgres_pipeline.cpp
        include/postgres_pipeline.hpp
)

target_include_directories(${DMP_DATABASE_POSTGRESQL}.Capabilities.Pipeline PUBLIC
        include/
)

target_link_libraries(${DMP_DATABASE_POSTGRESQL}.Capabilities.Pipeline PUBLIC

)
##############################################################################