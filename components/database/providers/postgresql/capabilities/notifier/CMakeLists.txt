##############################################################################
#
##############################################################################
add_library(${DMP_DATABASE_POSTGRESQL}.Capabilities.Notifier STATIC
        source/postgres_notifier.cpp
        include/postgres_notifier.hpp
)

target_include_directories(${DMP_DATABASE_POSTGRESQL}.Capabilities.Notifier PUBLIC
        include/
)

target_link_libraries(${DMP_DATABASE_POSTGRESQL}.Capabilities.Notifier PUBLIC

)
##############################################################################