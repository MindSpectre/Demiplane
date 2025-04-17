set(DMP_MATH ${DMP_CORE}.Math)

##############################################################################
# Math utilities
##############################################################################
add_library(${DMP_MATH}.Random STATIC
        random/source/random.cpp
)

target_include_directories(${DMP_MATH}.Random
        PUBLIC
        random/include/
)
##############################################################################

##############################################################################
# Math utilities
##############################################################################
add_library(${DMP_MATH}.Utilities INTERFACE)

target_include_directories(${DMP_MATH}.Utilities
        INTERFACE
        utilities/
)
##############################################################################



add_combined_library(${DMP_MATH}
        DIRECTORIES
        export
        LIBRARIES
        ${DMP_MATH}.Utilities
        ${DMP_MATH}.Random
        ALIAS
        Demiplane::Math
)