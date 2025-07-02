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
        core/entry_interface.hpp
        core/source/custom_entry.cpp
        core/log_level.hpp
        core/include/entry/factory/entry_factory.hpp
)
target_include_directories(${DMP_SCROLL}.Core PUBLIC
        core/include
)
target_link_libraries(${DMP_SCROLL}.Core PUBLIC
        JsonCpp::JsonCpp
        Demiplane::Common::Chrono
        Demiplane::Common::Gears
        ${DMP_SCROLL}.Colors
)

##############################################################################


##############################################################################
# Scroll Loggers
##############################################################################
add_library(${DMP_SCROLL}.Logger STATIC
        logger/source/logger.cpp
        logger/include/log_macros.hpp
        logger/include/file_logger.hpp
        logger/include/console_logger.hpp
)
target_include_directories(${DMP_SCROLL}.Logger PUBLIC
        logger/include
)
target_link_libraries(${DMP_SCROLL}.Logger PUBLIC
        Demiplane::3rdParty::ConcurrentQueue
        ${DMP_SCROLL}.Core
)
##############################################################################


##############################################################################
# Scroll logger provider
##############################################################################
add_library(${DMP_SCROLL}.Logger.Provider STATIC
        logger_provider/source/logger_provider.cpp
        logger_provider/include/logger_provider.hpp
        logger_provider/include/log_macros_adds.hpp
)
target_include_directories(${DMP_SCROLL}.Logger.Provider PUBLIC
        logger_provider/include
)

target_link_libraries(${DMP_SCROLL}.Logger.Provider PUBLIC
        ${DMP_SCROLL}.Logger
)
##############################################################################

##############################################################################
# Scroll exported lib
##############################################################################
add_combined_library(${DMP_SCROLL}
        DIRECTORIES
        export
        LIBRARIES
        ${DMP_SCROLL}.Logger.Provider
)
add_library(Demiplane::Common::Scroll ALIAS ${DMP_SCROLL})
##############################################################################