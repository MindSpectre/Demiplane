set(DMP_SCROLL ${DMP_COMMON}.Scroll)

##############################################################################
# Scroll Colors
##############################################################################
add_library(${DMP_SCROLL}.Colors INTERFACE
        colors/colors.hpp
        core/include/entry/fast_detailed_entry.hpp
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
        core/include/entry/fast_detailed_entry.hpp
)
target_include_directories(${DMP_SCROLL}.Core PUBLIC
        core/include
)
target_link_libraries(${DMP_SCROLL}.Core PUBLIC
        JsonCpp::JsonCpp
        Demiplane::Common::Nexus
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
        logger/source/file_logger.inl
)
target_include_directories(${DMP_SCROLL}.Logger PUBLIC
        logger/include
)
target_link_libraries(${DMP_SCROLL}.Logger PUBLIC
        Demiplane::3rdParty::ConcurrentQueue
        Demiplane::Common::Algorithms
        ${DMP_SCROLL}.Core
)
##############################################################################


##############################################################################
# Scroll logger provider
##############################################################################
add_library(${DMP_SCROLL}.Logger.Provider STATIC
        provider/source/logger_provider.cpp
        provider/include/logger_provider.hpp
        provider/include/log_macros_adds.hpp
)
target_include_directories(${DMP_SCROLL}.Logger.Provider PUBLIC
        provider/include
)

target_link_libraries(${DMP_SCROLL}.Logger.Provider
        PUBLIC
        ${DMP_SCROLL}.Logger
)
##############################################################################

##############################################################################
# Scroll exported lib
##############################################################################
add_combined_library(${DMP_SCROLL}
        DIRECTORIES
        export
        SOURCES
        export/demiplane/scroll
        LIBRARIES
        ${DMP_SCROLL}.Logger.Provider
)

add_library(Demiplane::Common::Scroll ALIAS ${DMP_SCROLL})
##############################################################################