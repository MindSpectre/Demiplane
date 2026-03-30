##############################################################################
#
##############################################################################
add_library(${DMP_DATABASE_POSTGRESQL}.Capabilities.Notifier STATIC
        postgres_notifier.cpp
        postgres_notifier.hpp
)

target_include_directories(${DMP_DATABASE_POSTGRESQL}.Capabilities.Notifier PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${DMP_DATABASE_POSTGRESQL}.Capabilities.Notifier PUBLIC

)
##############################################################################