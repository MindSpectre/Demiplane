set(DMP_GEARS ${DMP_COMMON}.Gears)

##############################################################################
# Result class for convenient handling
##############################################################################
add_library(${DMP_GEARS}.Result INTERFACE
        outcome/gears_outcome.hpp
)
target_include_directories(${DMP_GEARS}.Result INTERFACE
        outcome/
)
target_link_libraries(${DMP_GEARS}.Result INTERFACE
        ${DMP_GEARS}.MetaProgramming
)
##############################################################################

##############################################################################
# Custom string library
##############################################################################
add_library(${DMP_GEARS}.Strings INTERFACE
        strings/gears_strings.hpp
)
target_include_directories(${DMP_GEARS}.Strings INTERFACE
        strings/
)
##############################################################################

##############################################################################
# Traits for classes
##############################################################################
add_library(${DMP_GEARS}.MetaProgramming INTERFACE
        meta_programming/gears_class_traits.hpp
        meta_programming/gears_templates.hpp
        meta_programming/gears_concepts.hpp
)
target_include_directories(${DMP_GEARS}.MetaProgramming INTERFACE
        meta_programming/
)
##############################################################################

##############################################################################
# Traits for classes
##############################################################################
add_library(${DMP_GEARS}.Utilities INTERFACE
        utilities/gears_macros.hpp
        utilities/gears_utils.hpp
        utilities/gears_types.hpp
        utilities/gears_hash.hpp
        utilities/gears_convert.hpp
)
target_include_directories(${DMP_GEARS}.Utilities INTERFACE
        utilities/
)
target_link_libraries(${DMP_GEARS}.Utilities INTERFACE
        ${DMP_GEARS}.MetaProgramming
)
##############################################################################

##############################################################################
# Gears exported library
##############################################################################
add_combined_library(${DMP_GEARS}
        DIRECTORIES
        export
        SOURCES
        export/demiplane/gears
        LIBRARIES
        ${DMP_GEARS}.Result
        ${DMP_GEARS}.MetaProgramming
        ${DMP_GEARS}.Utilities
        ${DMP_GEARS}.Strings
)

add_library(Demiplane::Common::Gears ALIAS ${DMP_GEARS})
##############################################################################
