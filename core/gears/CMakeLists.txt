set(DMP_GEARS ${DMP_CORE}.Gears)


###############################################
# Result class for convenient handling
###############################################
add_library(${DMP_GEARS}.Result INTERFACE)
target_include_directories(${DMP_GEARS}.Result INTERFACE
        result/include/
)
###############################################

###############################################
# Traits for classes
###############################################
add_library(${DMP_GEARS}.ClassTraits INTERFACE)
target_include_directories(${DMP_GEARS}.ClassTraits INTERFACE
        class_traits/include
)
###############################################

###############################################
# Traits for classes
###############################################
add_library(${DMP_GEARS}.Utilities INTERFACE)
target_include_directories(${DMP_GEARS}.Utilities INTERFACE
        utilities/include
)
###############################################

add_combined_library(${DMP_GEARS}
        DIRECTORIES
        export
        LIBRARIES
        ${DMP_GEARS}.Result
        ${DMP_GEARS}.ClassTraits
        ${DMP_GEARS}.Utilities
        ALIAS
        Demiplane::Gears
)
###############################################