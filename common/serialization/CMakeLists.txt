set(DMP_SERIALIZATION ${DMP_COMMON}.Serialization)

##############################################################################
# Field descriptor and concepts (header-only)
##############################################################################
add_library(${DMP_SERIALIZATION}.Config INTERFACE
        config/field.hpp
        config/serial_concepts.hpp
        config/config_interface.hpp
)
target_include_directories(${DMP_SERIALIZATION}.Config INTERFACE
        config
)
target_link_libraries(${DMP_SERIALIZATION}.Config INTERFACE
        Demiplane::Common::Gears
)
##############################################################################

##############################################################################
# JSON format support
##############################################################################
add_library(${DMP_SERIALIZATION}.Formats.Json STATIC
        formats/json/json.hpp
        formats/json/json.cpp
)
target_include_directories(${DMP_SERIALIZATION}.Formats.Json PUBLIC
        formats
)
target_link_libraries(${DMP_SERIALIZATION}.Formats.Json PUBLIC
        ${DMP_SERIALIZATION}.Config
        JsonCpp::JsonCpp
)
##############################################################################

##############################################################################
# Serialization exported library
##############################################################################
add_combined_library(${DMP_SERIALIZATION}
        DIRECTORIES
        export
        SOURCES
        export/demiplane/serialization
        LIBRARIES
        ${DMP_SERIALIZATION}.Config
        ${DMP_SERIALIZATION}.Formats.Json
)

add_library(Demiplane::Common::Serialization ALIAS ${DMP_SERIALIZATION})
##############################################################################
