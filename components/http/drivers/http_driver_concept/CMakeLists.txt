##############################################################################
# Http Drivers — HttpDriver concept (header-only)
##############################################################################
add_library(${DMP_HTTP}.Drivers.Concept INTERFACE
        http_driver_concept.hpp
)

target_include_directories(${DMP_HTTP}.Drivers.Concept INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${DMP_HTTP}.Drivers.Concept INTERFACE
        ${DMP_HTTP}.Types.Enums
)
##############################################################################
