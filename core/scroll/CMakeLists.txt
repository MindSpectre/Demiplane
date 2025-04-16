set(DMP_SCROLL ${DMP_CORE}.Scroll)

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
add_library(${DMP_SCROLL}.Entry STATIC
        entry/entry_config.hpp
        entry/entry_config.cpp
        entry/entry_processor.hpp
)
target_include_directories(${DMP_SCROLL}.Entry PUBLIC
        entry/
)
target_link_libraries(${DMP_SCROLL}.Entry PUBLIC
        JsonCpp::JsonCpp
        Demiplane::Chrono
        Demiplane::Gears

        ${DMP_SCROLL}.Colors
)

##############################################################################

##############################################################################
# Scroll Configs
##############################################################################
add_library(${DMP_SCROLL}.Config STATIC
        configurator/factory/scroll_config_factory.hpp
        configurator/factory/scroll_config_factory.cpp
        configurator/config_interface.hpp
        configurator/configs/file_tracer_config.hpp
        configurator/configs/console_tracer_config.hpp
)
target_include_directories(${DMP_SCROLL}.Config PUBLIC
        configurator/factory
)
target_link_libraries(${DMP_SCROLL}.Config PUBLIC
        ${DMP_SCROLL}.Entry
)

##############################################################################

##############################################################################
# Scroll Tracers
##############################################################################
add_library(${DMP_SCROLL}.Tracer STATIC
        tracers/tracer_base.hpp
        tracers/file_tracer.hpp
        tracers/console_tracer.hpp
        tracers/tracer_factory.hpp
)
target_include_directories(${DMP_SCROLL}.Tracer PUBLIC
        tracers/include
)
target_link_libraries(${DMP_SCROLL}.Tracer PUBLIC
        ${DMP_SCROLL}.Config
)
add_library(Demiplane::Scroll::Tracer ALIAS ${DMP_SCROLL}.Tracer)

##############################################################################


###############################################################################
## Scroll Loggers
###############################################################################
#add_library(${DMP_SCROLL}.Logger STATIC
#        loggers/logger_interface.hpp
#        loggers/file_logger.hpp
#        loggers/console_logger.hpp
#        loggers/factory/scroll_logger.hpp
#)
#target_include_directories(${DMP_SCROLL}.Logger PUBLIC
#        loggers/factory
#)
#target_link_libraries(${DMP_SCROLL}.Logger PUBLIC
#        Demiplane::Scroll::Config
#)
#add_library(Demiplane::Scroll::Logger ALIAS ${DMP_SCROLL}.Logger)
#
###############################################################################
add_combined_library(${DMP_SCROLL}
        DIRECTORIES
        export
        LIBRARIES
#        Demiplane::Scroll::Logger
        Demiplane::Scroll::Tracer
        ALIAS
        Demiplane::Scroll
)