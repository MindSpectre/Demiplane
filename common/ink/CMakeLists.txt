set(DMP_INK ${DMP_COMMON}.Ink)

##############################################################################
# Ink Base - colors, glyphs, separators
##############################################################################
add_library(${DMP_INK}.Base INTERFACE
        base/separators.hpp
        base/colors.hpp
        base/glyphs.hpp
)
target_include_directories(${DMP_INK}.Base INTERFACE base)
target_link_libraries(${DMP_INK}.Base INTERFACE
        Demiplane::Common::Gears
)
##############################################################################

add_subdirectory(rendering)

##############################################################################
# Ink exported library
##############################################################################
add_combined_library(${DMP_INK}
        DIRECTORIES
        export
        SOURCES
        export/demiplane/ink
        LIBRARIES
        ${DMP_INK}.Base
        ${DMP_INK}.Table
        ${DMP_INK}.Section
        ${DMP_INK}.Box
)

add_library(Demiplane::Common::Ink ALIAS ${DMP_INK})
##############################################################################
