##############################################################################
# Http Server — attach_default_listeners (config-driven listener wiring)
##############################################################################
add_library(${DMP_HTTP}.Server.AttachDefaultListeners STATIC
        attach_default_listeners.cpp
        attach_default_listeners.hpp
)

target_include_directories(${DMP_HTTP}.Server.AttachDefaultListeners PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${DMP_HTTP}.Server.AttachDefaultListeners
        PUBLIC
        ${DMP_HTTP}.Server.Core
        ${DMP_HTTP}.Config
        ${DMP_HTTP}.Drivers
)
##############################################################################
