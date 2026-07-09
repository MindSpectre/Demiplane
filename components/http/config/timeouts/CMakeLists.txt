##############################################################################
# Http Config — Timeouts (per-phase timeout set; spec §10.1)
##############################################################################
add_library(${DMP_HTTP}.Config.Timeouts INTERFACE timeouts.hpp)

target_include_directories(${DMP_HTTP}.Config.Timeouts INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${DMP_HTTP}.Config.Timeouts INTERFACE
        Demiplane::Common::Serialization
)
##############################################################################
