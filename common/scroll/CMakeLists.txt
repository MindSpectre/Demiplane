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
        entry/include/entry_interface.hpp
        entry/include/log_level.hpp
        entry/include/factory/entry_factory.hpp
        entry/include/detailed_entry.hpp
        entry/include/light_entry.hpp
        entry/source/detailed_entry.cpp
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
)

##############################################################################


##############################################################################
# Logger Interface Meta
##############################################################################
add_library(${DMP_SCROLL}.Sink.Interface INTERFACE
        sink/interface/sink_interface.hpp
        sink/interface/log_event.hpp
)

target_include_directories(${DMP_SCROLL}.Sink.Interface INTERFACE
        sink/interface
)

target_link_libraries(${DMP_SCROLL}.Sink.Interface INTERFACE
        ${DMP_SCROLL}.Entry
)
##############################################################################

##############################################################################
# Scroll Loggers
##############################################################################
add_library(${DMP_SCROLL}.Sink.File STATIC
        sink/file_sink/include/file_sink.hpp
)
target_include_directories(${DMP_SCROLL}.Sink.File PUBLIC
        sink/file_sink/include
)
target_link_libraries(${DMP_SCROLL}.Sink.File PUBLIC
        ${DMP_SCROLL}.Sink.Interface
)
##############################################################################

##############################################################################
# Scroll Loggers
##############################################################################
add_library(${DMP_SCROLL}.Sink.Console STATIC
        sink/console_sink/include/console_sink.hpp
)
target_include_directories(${DMP_SCROLL}.Sink.Console PUBLIC
        sink/console_sink/include/
)
target_link_libraries(${DMP_SCROLL}.Sink.Console PUBLIC
        ${DMP_SCROLL}.Sink.Interface
        ${DMP_SCROLL}.Colors
)
##############################################################################

##############################################################################
# Scroll logger
##############################################################################
add_library(${DMP_SCROLL}.Logger STATIC
        logger/include/logger.hpp
)
target_include_directories(${DMP_SCROLL}.Logger PUBLIC
        logger/include
)

target_link_libraries(${DMP_SCROLL}.Logger
        PUBLIC
        ${DMP_SCROLL}.Sink.Console
        ${DMP_SCROLL}.Sink.File
        Demiplane::Common::Multithread::Disruptor
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