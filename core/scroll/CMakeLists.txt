set(SCROLL Scroll)

##############################################################################
# Scroll Colors
##############################################################################
add_library(${PROJECT_NAME}.${SCROLL}.Colors INTERFACE
        colors/colors.hpp
)
target_include_directories(${PROJECT_NAME}.${SCROLL}.Colors INTERFACE colors)

add_library(Demiplane::Scroll::Colors ALIAS ${PROJECT_NAME}.${SCROLL}.Colors)

##############################################################################

##############################################################################
# Scroll Entry creator
##############################################################################
add_library(${PROJECT_NAME}.${SCROLL}.Entry STATIC
        entry/entry_config.hpp
        entry/entry_config.cpp
        entry/entry_processor.hpp
)
target_include_directories(${PROJECT_NAME}.${SCROLL}.Entry PUBLIC
        entry/
)
target_link_libraries(${PROJECT_NAME}.${SCROLL}.Entry PUBLIC
        JsonCpp::JsonCpp
        Demiplane::Utilities
        Demiplane::Result
        Demiplane::Scroll::Colors
        Demiplane::Traits
)
add_library(Demiplane::Scroll::Entry ALIAS ${PROJECT_NAME}.${SCROLL}.Entry)

##############################################################################

##############################################################################
# Scroll Configs
##############################################################################
add_library(${PROJECT_NAME}.${SCROLL}.Config STATIC
        configurator/factory/scroll_config_factory.hpp
        configurator/factory/scroll_config_factory.cpp
        configurator/config_interface.hpp
        configurator/configs/file_tracer_config.hpp
        configurator/configs/console_tracer_config.hpp
)
target_include_directories(${PROJECT_NAME}.${SCROLL}.Config PUBLIC
        configurator/factory
)
target_link_libraries(${PROJECT_NAME}.${SCROLL}.Config PUBLIC
        Demiplane::Scroll::Entry
)
add_library(Demiplane::Scroll::Config ALIAS ${PROJECT_NAME}.${SCROLL}.Config)

##############################################################################

##############################################################################
# Scroll Tracers
##############################################################################
add_library(${PROJECT_NAME}.${SCROLL}.Tracer STATIC
        tracers/tracer_interface.hpp
        tracers/file_tracer.hpp
        tracers/console_tracer.hpp
        tracers/factory/scroll_tracer.cpp
        tracers/factory/scroll_tracer.hpp
)
target_include_directories(${PROJECT_NAME}.${SCROLL}.Tracer PUBLIC
        tracers/factory
)
target_link_libraries(${PROJECT_NAME}.${SCROLL}.Tracer PUBLIC
        Demiplane::Scroll::Config
)
add_library(Demiplane::Scroll::Tracer ALIAS ${PROJECT_NAME}.${SCROLL}.Tracer)

##############################################################################


##############################################################################
# Scroll Loggers
##############################################################################
add_library(${PROJECT_NAME}.${SCROLL}.Logger STATIC
        loggers/logger_interface.hpp
        loggers/file_logger.hpp
        loggers/console_logger.hpp
        loggers/factory/logger_factory.cpp
        loggers/factory/scroll_logger.hpp
)
target_include_directories(${PROJECT_NAME}.${SCROLL}.Logger PUBLIC
        loggers/factory
)
target_link_libraries(${PROJECT_NAME}.${SCROLL}.Logger PUBLIC
        Demiplane::Scroll::Config
)
add_library(Demiplane::Scroll::Logger ALIAS ${PROJECT_NAME}.${SCROLL}.Logger)

##############################################################################

add_library(${PROJECT_NAME}.${SCROLL} INTERFACE)
target_link_libraries(${PROJECT_NAME}.${SCROLL} INTERFACE
        Demiplane::Scroll::Logger
        Demiplane::Scroll::Tracer
)
add_library(Demiplane::Scroll ALIAS ${PROJECT_NAME}.${SCROLL})