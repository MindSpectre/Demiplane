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
# Scroll Core
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