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
# Scroll Entry
##############################################################################
add_library(${DMP_SCROLL}.Entry STATIC
        entry/entry_interface.hpp
        entry/log_level.hpp
        entry/include/factory/entry_factory.hpp
        entry/include/detailed_entry.hpp
        entry/include/light_entry.hpp
        entry/include/service_entry.hpp
        entry/include/custom_entry.hpp
        entry/source/detailed_entry.cpp
        entry/source/custom_entry.cpp
)
target_include_directories(${DMP_SCROLL}.Entry PUBLIC
        entry/include
)
target_link_libraries(${DMP_SCROLL}.Entry PUBLIC
        JsonCpp::JsonCpp
        Demiplane::Common::Nexus
        Demiplane::Common::Gears
        Demiplane::Common::Chrono
        PRIVATE
        ${DMP_SCROLL}.Colors
)

##############################################################################


##############################################################################
# Scroll Loggers
##############################################################################
add_library(${DMP_SCROLL}.Logger STATIC
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
        ${DMP_SCROLL}.Entry
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