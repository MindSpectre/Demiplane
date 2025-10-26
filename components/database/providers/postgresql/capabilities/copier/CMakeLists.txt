##############################################################################
#
##############################################################################
add_library(${DMP_DATABASE_POSTGRESQL}.Capabilities.Copier STATIC
        source/postgres_copier.cpp
        include/postgres_copier.hpp
)

target_include_directories(${DMP_DATABASE_POSTGRESQL}.Capabilities.Copier PUBLIC
        include/
)

target_link_libraries(${DMP_DATABASE_POSTGRESQL}.Capabilities.Copier PUBLIC

)
##############################################################################