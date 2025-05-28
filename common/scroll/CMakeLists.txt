set(DMP_SCROLL ${DMP_COMMON}.Scroll)

##############################################################################
# Scroll Colors
##############################################################################
add_library(${DMP_SCROLL}.Colors INTERFACE
        colors/colors.hpp
)
target_include_directories(${DMP_SCROLL}.Colors INTERFACE colors)

##############################################################################

##############################################################################
# Scroll Entry creator
##############################################################################
add_library(${DMP_SCROLL}.Core STATIC
        core/entry/entry_interface.hpp
        core/entry/custom/custom_entry.cpp
        core/log_level.hpp
)
target_include_directories(${DMP_SCROLL}.Core PUBLIC
        core/
        core/entry/basic
        core/entry/custom
)
target_link_libraries(${DMP_SCROLL}.Core PUBLIC
        JsonCpp::JsonCpp
        Demiplane::Chrono
        Demiplane::Gears
        ${DMP_SCROLL}.Colors
)

##############################################################################


##############################################################################
###############################################################################
## Scroll Configs
###############################################################################
#add_library(${DMP_SCROLL}.Config STATIC
#        configurator/factory/scroll_config_factory.hpp
#        configurator/factory/scroll_config_factory.cpp
#        configurator/config_interface.hpp
#        configurator/configs/file_tracer_config.hpp
#        configurator/configs/console_tracer_config.hpp
#)
#target_include_directories(${DMP_SCROLL}.Config PUBLIC
#        configurator/factory
#)
#target_link_libraries(${DMP_SCROLL}.Config PUBLIC
#        ${DMP_SCROLL}.Entry
#)
#
###############################################################################

###############################################################################
## Scroll Tracers
###############################################################################
#add_library(${DMP_SCROLL}.Tracer STATIC
#        tracers/tracer_base.hpp
#        tracers/file_tracer.hpp
#        tracers/console_tracer.hpp
#        tracers/tracer_factory.hpp
#)
#target_include_directories(${DMP_SCROLL}.Tracer PUBLIC
#        tracers/include
#)
#target_link_libraries(${DMP_SCROLL}.Tracer PUBLIC
#        ${DMP_SCROLL}.Config
#)
#add_library(Demiplane::Scroll::Tracer ALIAS ${DMP_SCROLL}.Tracer)
#
###############################################################################


##############################################################################
# Scroll Loggers
##############################################################################
add_library(${DMP_SCROLL}.Logger STATIC
    logger/source/logger.cpp
)
target_include_directories(${DMP_SCROLL}.Logger PUBLIC
        logger/include
)
target_link_libraries(${DMP_SCROLL}.Logger PUBLIC
        ${DMP_SCROLL}.Core
)

##############################################################################
add_combined_library(${DMP_SCROLL}
        DIRECTORIES
        export
        LIBRARIES
        ${DMP_SCROLL}.Logger
        ALIAS
        Demiplane::Scroll
)