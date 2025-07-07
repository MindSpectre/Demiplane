set(DMP_GEARS ${DMP_COMMON}.Gears)


##############################################################################
# Result class for convenient handling
##############################################################################
add_library(${DMP_GEARS}.Result INTERFACE
        result/include/result.hpp
)
target_include_directories(${DMP_GEARS}.Result INTERFACE
        result/include/
)
##############################################################################

##############################################################################
# Traits for classes
##############################################################################
add_library(${DMP_GEARS}.MetaProgramming INTERFACE
        meta_programming/include/class_traits.hpp
        meta_programming/include/templates.hpp
        meta_programming/include/concepts.hpp
)
target_include_directories(${DMP_GEARS}.MetaProgramming INTERFACE
        meta_programming/include
)
##############################################################################

##############################################################################
# Traits for classes
##############################################################################
add_library(${DMP_GEARS}.Utilities INTERFACE
        utilities/include/gears_macros.hpp
        utilities/include/gears_utils.hpp
        utilities/include/gears_types.hpp
)
target_include_directories(${DMP_GEARS}.Utilities INTERFACE
        utilities/include
)
##############################################################################

##############################################################################
# Gears exported library
##############################################################################
add_combined_library(${DMP_GEARS}
        DIRECTORIES
        export
        LIBRARIES
        ${DMP_GEARS}.Result
        ${DMP_GEARS}.MetaProgramming
        ${DMP_GEARS}.Utilities
)

add_library(Demiplane::Common::Gears ALIAS ${DMP_GEARS})
##############################################################################
