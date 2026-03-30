##############################################################################
#
##############################################################################
add_library(${DMP_DATABASE_POSTGRESQL}.Capabilities.Copier STATIC
        postgres_copier.cpp
        postgres_copier.hpp
)

target_include_directories(${DMP_DATABASE_POSTGRESQL}.Capabilities.Copier PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${DMP_DATABASE_POSTGRESQL}.Capabilities.Copier PUBLIC

)
##############################################################################