set(DMP_MATH ${DMP_COMMON}.Math)

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
# Math exported library
##############################################################################
add_combined_library(${DMP_MATH}
        DIRECTORIES
        export
        LIBRARIES
        ${DMP_MATH}.Random
)

add_library(Demiplane::Common::Math ALIAS ${DMP_MATH})
##############################################################################
