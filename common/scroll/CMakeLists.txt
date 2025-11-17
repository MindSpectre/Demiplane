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
        entry/include/custom_entry.hpp
        entry/source/detailed_entry.cpp
        entry/source/custom_entry.cpp
)
target_include_directories(${DMP_SCROLL}.Entry PUBLIC
        entry/include
)
target_link_libraries(${DMP_SCROLL}.Entry PUBLIC
        JsonCpp::JsonCpp
        Threads::Threads
        Demiplane::Common::Nexus
        Demiplane::Common::Gears
        Demiplane::Common::Chrono
        PRIVATE
        ${DMP_SCROLL}.Colors
)

##############################################################################


##############################################################################
# Logger Interface Meta
##############################################################################
add_library(${DMP_SCROLL}.Logger.Interface INTERFACE
        logger/interface/logger_interface.hpp
        logger/interface/stream_log.hpp
)

target_include_directories(${DMP_SCROLL}.Logger.Interface INTERFACE
    logger/interface
)

target_link_libraries(${DMP_SCROLL}.Logger.Interface INTERFACE
        ${DMP_SCROLL}.Entry
)
##############################################################################

##############################################################################
# Scroll Loggers
##############################################################################
add_library(${DMP_SCROLL}.Logger.File STATIC
        logger/file_logger/include/file_logger.hpp
        logger/file_logger/source/file_logger.inl
)
target_include_directories(${DMP_SCROLL}.Logger.File PUBLIC
        logger/file_logger/include
)
target_link_libraries(${DMP_SCROLL}.Logger.File PUBLIC
        Demiplane::3rdParty::ConcurrentQueue
        Demiplane::Common::Algorithms
        ${DMP_SCROLL}.Logger.Interface
)
##############################################################################

##############################################################################
# Scroll Loggers
##############################################################################
add_library(${DMP_SCROLL}.Logger.Console STATIC

        logger/console_logger/include/console_logger.hpp
)
target_include_directories(${DMP_SCROLL}.Logger.Console PUBLIC
        logger/console_logger/include
)
target_link_libraries(${DMP_SCROLL}.Logger.Console PUBLIC
        ${DMP_SCROLL}.Logger.Interface
)
##############################################################################


##############################################################################
# Scroll logger provider
##############################################################################
add_library(${DMP_SCROLL}.Logger.Provider STATIC
        provider/source/logger_provider.cpp
        provider/include/logger_provider.hpp
        provider/include/log_macros_adds.hpp
        provider/include/log_macros.hpp
)
target_include_directories(${DMP_SCROLL}.Logger.Provider PUBLIC
        provider/include
)

target_link_libraries(${DMP_SCROLL}.Logger.Provider
        PUBLIC
        ${DMP_SCROLL}.Logger.Console
        ${DMP_SCROLL}.Logger.File
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