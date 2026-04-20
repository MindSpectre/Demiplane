set(DMP_INK ${DMP_COMMON}.Ink)

##############################################################################
# Ink Colors — ANSI constants, Style, color-wrap helpers
##############################################################################
add_library(${DMP_INK}.Colors INTERFACE
        colors/codes.hpp
        colors/colors.hpp
)
target_include_directories(${DMP_INK}.Colors INTERFACE colors)
##############################################################################

##############################################################################
# Ink Separators — Align enum, newline/hr/indent/pad, detail::visible_width
##############################################################################
add_library(${DMP_INK}.Separators INTERFACE
        separators/separators.hpp
)
target_include_directories(${DMP_INK}.Separators INTERFACE separators)
##############################################################################

##############################################################################
# Ink Table — value-type fluent builder with column-DSL and row-DSL
##############################################################################
add_library(${DMP_INK}.Table INTERFACE
        table/table.hpp
)
target_include_directories(${DMP_INK}.Table INTERFACE table)
target_link_libraries(${DMP_INK}.Table INTERFACE
        ${DMP_INK}.Colors
        ${DMP_INK}.Separators
)
##############################################################################

##############################################################################
# Ink Section — title + (label, value) pairs, multi-line value alignment
##############################################################################
add_library(${DMP_INK}.Section INTERFACE
        section/section.hpp
)
target_include_directories(${DMP_INK}.Section INTERFACE section)
target_link_libraries(${DMP_INK}.Section INTERFACE
        ${DMP_INK}.Colors
        ${DMP_INK}.Separators
)
##############################################################################

##############################################################################
# Ink Box — titled bordered block around body
##############################################################################
add_library(${DMP_INK}.Box INTERFACE
        box/box.hpp
)
target_include_directories(${DMP_INK}.Box INTERFACE box)
target_link_libraries(${DMP_INK}.Box INTERFACE
        ${DMP_INK}.Colors
        ${DMP_INK}.Separators
)
##############################################################################

##############################################################################
# Ink exported library
##############################################################################
add_combined_library(${DMP_INK}
        DIRECTORIES
        export
        SOURCES
        export/demiplane/ink
        LIBRARIES
        ${DMP_INK}.Colors
        ${DMP_INK}.Separators
        ${DMP_INK}.Table
        ${DMP_INK}.Section
        ${DMP_INK}.Box
)

add_library(Demiplane::Common::Ink ALIAS ${DMP_INK})
##############################################################################
